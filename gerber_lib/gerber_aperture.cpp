//////////////////////////////////////////////////////////////////////

#include <format>
#include <stack>
#include <map>
#include <cmath>

#include "gerber_error.h"
#include "gerber_util.h"
#include "gerber_reader.h"
#include "gerber_state.h"
#include "gerber_aperture.h"

LOG_CONTEXT("aperture", info);

namespace
{
    using namespace gerber_lib;
    using namespace gerber_util;

    //////////////////////////////////////////////////////////////////////

    enum gerber_associativity
    {
        associate_left = 0,
        associate_right = 1,
    };

    //////////////////////////////////////////////////////////////////////

    struct opcode_descriptor
    {
        gerber_associativity associativity;
        int operator_precedence;
        bool precedes_unary_operator;
    };

    // if previous token is:
    // open bracket
    // first token (i.e. nop)
    // an operator (X,/,+,-)
    // then +,- are unary

    opcode_descriptor opcode_details[] = {
        { associate_left, 0, true },     // opcode_nop
        { associate_left, 0, false },    // opcode_push_value
        { associate_left, 0, false },    // opcode_push_parameter
        { associate_left, 0, false },    // opcode_pop_parameter
        { associate_left, 1, true },     // opcode_add
        { associate_left, 1, true },     // opcode_subtract
        { associate_left, 2, true },     // opcode_multiply
        { associate_left, 2, true },     // opcode_divide
        { associate_right, 0, true },    // opcode_open_bracket
        { associate_left, 0, true },     // opcode_close_bracket
        { associate_right, 3, true },    // opcode_unary_minus
        { associate_right, 3, true },    // opcode_unary_plus
        { associate_left, 0, false },    // opcode_primitive
    };

    static_assert(array_length(opcode_details) == opcode_num_opcodes);

}    // namespace

namespace gerber_lib
{
    //////////////////////////////////////////////////////////////////////
    // parse_aperture_macro should eat the trailing %

    gerber_error_code gerber_aperture_macro::parse_aperture_macro(gerber_reader &reader)
    {
        LOG_CONTEXT("macro_parser", info);

        CHECK(reader.read_until(&name, '*'));

        LOG_VERBOSE("Aperture macro: {}", name);

        // skip '*'
        char c;
        CHECK(reader.read_char(&c));

        if(c != '*' || name.empty()) {
            return error_bad_macro_name;
        }

        constexpr size_t max_operation_stack_size = 64;

        // have we seen the terminator (%) yet
        bool done{ false };

        // have we parsed the first bit yet (either "number," or "$number=")
        bool got_line_header{ false };

        // if it started with "number," then this is the number
        int primitive{ 0 };

        // if it started with "$number=" then this is the number
        int assign_parameter{ 0 };

        // does previous opcode mean a + or - is interpreted as unary
        bool unary_available{ true };

        // rpn stack of operators
        std::stack<gerber_opcode> ops;

        // flush opcode stack back to some opcode taking into account precedence, braces and left/right associativity

        auto flush_stack = [&](gerber_opcode opcode = opcode_nop) {
            while(!ops.empty()) {

                gerber_opcode next_opcode = ops.top();

                if(next_opcode == opcode_open_bracket) {
                    ops.pop();
                    break;
                }

                bool less{ false };

                opcode_descriptor const &details = opcode_details[opcode];
                opcode_descriptor const &next_details = opcode_details[next_opcode];

                switch(details.associativity) {

                case associate_left:
                    less = next_details.operator_precedence < details.operator_precedence;
                    break;

                default:
                    less = next_details.operator_precedence <= details.operator_precedence;
                    break;
                }

                if(less) {
                    break;
                }

                instructions.emplace_back(next_opcode);
                ops.pop();
            }
        };

        // push an operator onto the rpn stack

        auto push_opcode = [&](gerber_opcode opcode) {
            if(ops.size() >= max_operation_stack_size) {
                return error_formula_too_complex;
            }
            ops.push(opcode);
            unary_available = opcode_details[opcode].precedes_unary_operator;
            return ok;
        };

        // parse the macro definition

        while(!done) {

            char character;
            CHECK(reader.read_char(&character));

            switch(character) {

                // $ might be assigning a new variable (e.g. "$n=<expression>")
                // or it might be being used as part of an expression (e.g. "$n+5")

            case '$':

                if(got_line_header) {
                    // it's being used as part of an expression
                    int param;
                    CHECK(reader.get_int(&param));
                    instructions.emplace_back(opcode_push_parameter, param);
                    unary_available = false;
                } else {
                    // it's an assignment, `got_line_header` will be set when the = is found
                    CHECK(reader.get_int(&assign_parameter));
                }
                break;

            case '=':

                if(assign_parameter == 0) {
                    return error_invalid_aperture_macro;
                }
                unary_available = true;
                got_line_header = true;
                break;

            case ',':

                // no commas in assignment expressions
                if(assign_parameter != 0) {
                    return error_invalid_aperture_macro;
                }

                // must specify primitive before first comma
                if(primitive == 0) {
                    return error_invalid_aperture_macro;
                }
                unary_available = true;

                // first comma means preceding number was the primitive code
                if(!got_line_header) {
                    got_line_header = true;
                    break;
                }
                // else flush the entire stack, new expression or eol incoming
                flush_stack();
                assign_parameter = 0;
                break;

                // * is terminator, not multiply (which is x or X)!
                // might be terminating an assigment or a primitive

            case '*':

                if(!got_line_header) {
                    return error_invalid_aperture_macro;
                }

                // flush the stack
                while(!ops.empty()) {
                    gerber_opcode opcode = ops.top();
                    ops.pop();
                    instructions.emplace_back(opcode);
                }

                // this was either an assignment or a primitive

                if(assign_parameter != 0) {
                    LOG_DEBUG("assign: {}", assign_parameter);
                    instructions.emplace_back(opcode_pop_parameter, assign_parameter);
                } else {
                    LOG_DEBUG("primitive: {} ({})", primitive, static_cast<gerber_primitive_code>(primitive));
                    instructions.emplace_back(opcode_primitive, primitive);
                }
                assign_parameter = 0;
                primitive = 0;
                got_line_header = false;
                unary_available = true;
                break;

            case '(':

                // syntax: can't have open bracket after a literal or reference
                if(!ops.empty() && (ops.top() == opcode_push_parameter || ops.top() == opcode_push_value)) {
                    return error_syntax_error;
                } else {
                    CHECK(push_opcode(opcode_open_bracket));
                }
                break;

            case ')':

                // syntax: can't have close bracket after anything except a literal or reference
                if(ops.empty() || (ops.top() != opcode_push_parameter && ops.top() != opcode_push_value)) {
                    return error_syntax_error;
                } else {
                    flush_stack(opcode_close_bracket);
                    unary_available = false;
                }
                break;

            case '+':

                if(unary_available) {
                    CHECK(push_opcode(opcode_unary_plus));
                } else {
                    flush_stack(opcode_add);
                    CHECK(push_opcode(opcode_add));
                }
                break;

            case '-':

                if(unary_available) {
                    CHECK(push_opcode(opcode_unary_minus));
                } else {
                    flush_stack(opcode_subtract);
                    CHECK(push_opcode(opcode_subtract));
                }
                break;

            case '/':

                flush_stack(opcode_divide);
                CHECK(push_opcode(opcode_divide));
                break;

            case 'x':
            case 'X':

                flush_stack(opcode_multiply);
                CHECK(push_opcode(opcode_multiply));
                break;

            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '.':

                // Comments in aperture macros are a definition starting with zero and ending with a '*'
                if(character == '0' && !got_line_header && primitive == 0) {
                    // Comment continues until next '*', just throw it away.
                    std::string comment;
                    CHECK(reader.read_until(&comment, '*'));
                    LOG_VERBOSE("macro comment: {}", comment);
                    CHECK(reader.read_char(&character));    // skip the '*'
                    if(character != '*') {
                        return error_unterminated_command;
                    }
                    break;
                }

                // First number in an aperture macro describes the primitive as a numerical value
                if(!got_line_header) {

                    // must be an integer though
                    if(character == '.') {
                        return error_invalid_aperture_macro;
                    }
                    primitive = primitive * 10 + (character - '0');
                    break;
                }

                // already had the primitive, this is just some number in some expression
                reader.rewind(1);
                double d;
                CHECK(reader.get_double(&d));
                instructions.emplace_back(opcode_push_value, d);
                unary_available = false;
                break;

            case '%':
                done = true;
                LOG_DEBUG("Finished parsing {}, {} instructions", name, instructions.size());
                break;

            default:
                break;
            }
        }
        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    gerber_error_code gerber_aperture::execute_aperture_macro(double scale)
    {
        LOG_CONTEXT("execute_aperture_macro", info);

        LOG_DEBUG("Execute aperture macro \"{}\"", aperture_macro->name);

        int num_of_parameters{ 0 };
        bool clear_operator_used{ false };
        gerber_aperture_type type{ aperture_type_none };

        std::vector<double> macro_stack;

        auto stack_string = [&]() { return std::format("[{}]", join(macro_stack, ",")); };

        auto pop = [&](double *d) {
            if(d == nullptr) {
                return error_internal_bad_pointer;
            }
            if(macro_stack.empty()) {
                LOG_ERROR("stack underflow in execute_aperture_macro");
                return error_expression_stack_underflow;
            }
            double t = macro_stack.back();
            macro_stack.pop_back();
            *d = t;
            return ok;
        };

        for(size_t i = 0; i < parameters.size(); ++i) {
            LOG_VERBOSE("parameter ${}={}", i + 1, parameters[i]);
        }

        for(auto const &instruction : aperture_macro->instructions) {

            // LOG_DEBUG("Instruction: {}, stack has {} entries", get_opcode_name(instruction.opcode), macro_stack.size());

            switch(instruction.opcode) {

            case opcode_nop:
                break;

            case opcode_push_value:
                macro_stack.push_back(instruction.double_value);
                LOG_DEBUG("push value {} : {}", instruction.double_value, stack_string());
                break;

            case opcode_push_parameter: {
                if(instruction.int_value <= 0 || static_cast<size_t>(instruction.int_value) > parameters.size()) {
                    return error_bad_parameter_index;
                }
                double v = parameters[instruction.int_value - 1llu];
                macro_stack.push_back(v);
                LOG_DEBUG("push parameter {} (= {}) : {}", instruction.int_value, v, stack_string());
            } break;

            case opcode_pop_parameter: {
                double d;
                CHECK(pop(&d));
                int id = instruction.int_value - 1;
                if(id < 0) {
                    return error_bad_parameter_index;
                }
                if(parameters.size() <= static_cast<size_t>(id)) {
                    parameters.resize(static_cast<size_t>(id + 1));
                }
                parameters[id] = d;
                LOG_DEBUG("pop parameter (${} = {}) : {}", id + 1, parameters[id], stack_string());
            } break;

            case opcode_unary_plus: {
                double a;
                CHECK(pop(&a));
                macro_stack.push_back(a);
                LOG_DEBUG("plusify (nop?) ({}) : {}", a, stack_string());
            } break;

            case opcode_unary_minus: {
                double a;
                CHECK(pop(&a));
                macro_stack.push_back(-a);
                LOG_DEBUG("negate ({}) : {}", a, stack_string());
            } break;

            case opcode_add: {
                double a, b;
                CHECK(pop(&a));
                CHECK(pop(&b));
                macro_stack.push_back(a + b);
                LOG_DEBUG("add ({},{} = {}) : {}", a, b, a + b, stack_string());
            } break;

            case opcode_subtract: {
                double a, b;
                CHECK(pop(&b));
                CHECK(pop(&a));
                macro_stack.push_back(a - b);
                LOG_DEBUG("subtract ({},{} = {}) : {}", a, b, a - b, stack_string());
            } break;

            case opcode_multiply: {
                double a, b;
                CHECK(pop(&a));
                CHECK(pop(&b));
                macro_stack.push_back(a * b);
                LOG_DEBUG("multiply ({},{} = {}) : {}", a, b, a * b, stack_string());
            } break;

            case opcode_divide: {
                double a, b;
                CHECK(pop(&b));
                CHECK(pop(&a));
                macro_stack.push_back(a / b);
                LOG_DEBUG("divide ({},{} = {}) : {}", a, b, a / b, stack_string());
            } break;

            case opcode_primitive:

                switch(instruction.int_value) {

                case 1:
                    type = aperture_type_macro_circle;
                    num_of_parameters = circle_num_parameters;

                    // last parameter for circle (rotation) is optional...
                    if(macro_stack.size() == circle_num_parameters - 1llu) {
                        num_of_parameters = circle_num_parameters - 1llu;
                    }
                    break;

                case 4:
                    type = aperture_type_macro_outline;
                    num_of_parameters = (static_cast<int>(macro_stack[1]) + 1llu) * 2 + 3;
                    if(num_of_parameters < 0 || num_of_parameters >= INT_MAX / 4) {
                        return error_bad_parameter_count;
                    }
                    break;

                case 5:
                    type = aperture_type_macro_polygon;
                    num_of_parameters = polygon_num_parameters;
                    break;

                case 6:
                    type = aperture_type_macro_moire;
                    num_of_parameters = moire_num_parameters;
                    break;

                case 7:
                    type = aperture_type_macro_thermal;
                    num_of_parameters = thermal_num_parameters;
                    break;


                case 2:
                case 20:
                    type = aperture_type_macro_line20;
                    num_of_parameters = line_20_num_parameters;
                    break;


                case 21:
                    type = aperture_type_macro_line21;
                    num_of_parameters = line_21_num_parameters;
                    break;


                case 22:
                    type = aperture_type_macro_line22;
                    num_of_parameters = line_22_num_parameters;
                    break;

                default:
                    type = aperture_type_none;
                    num_of_parameters = 0;
                    LOG_ERROR("Invalid primitive: {}", instruction.int_value);
                    break;
                }

                LOG_DEBUG("Aperture: {}, {} parameters : {}", type, num_of_parameters, stack_string());

                if(type != aperture_type_none) {

                    if(num_of_parameters < 0 || num_of_parameters > max_num_aperture_parameters) {
                        return error_bad_parameter_count;
                    }

                    gerber_macro_parameters *macro = new gerber_macro_parameters();

                    macro->aperture_type = type;
                    macro->parameters.resize(num_of_parameters);

                    for(intptr_t i = num_of_parameters - 1; i >= 0; i -= 1) {
                        CHECK(pop(&macro->parameters[i]));
                    }

                    double exposure = 1.0;

                    // Convert any mm values to inches.
                    switch(type) {

                    case aperture_type_macro_circle:
                        exposure = macro->parameters[circle_exposure];
                        macro->parameters[circle_diameter] *= scale;
                        macro->parameters[circle_centre_x] *= scale;
                        macro->parameters[circle_centre_y] *= scale;
                        break;

                    case aperture_type_macro_outline:
                        exposure = macro->parameters[outline_exposure];
                        for(int i = 2; i < num_of_parameters - 1; ++i) {
                            macro->parameters[i] *= scale;
                        }
                        break;

                    case aperture_type_macro_polygon:
                        exposure = macro->parameters[polygon_exposure];
                        macro->parameters[polygon_centre_x] *= scale;
                        macro->parameters[polygon_centre_y] *= scale;
                        macro->parameters[polygon_diameter] *= scale;
                        break;

                    case aperture_type_macro_moire:
                        macro->parameters[moire_centre_x] *= scale;
                        macro->parameters[moire_centre_y] *= scale;
                        macro->parameters[moire_outside_diameter] *= scale;
                        macro->parameters[moire_circle_line_width] *= scale;
                        macro->parameters[moire_gap_width] *= scale;
                        macro->parameters[moire_crosshair_line_width] *= scale;
                        macro->parameters[moire_crosshair_length] *= scale;
                        break;

                    case aperture_type_macro_thermal:
                        macro->parameters[thermal_centre_x] *= scale;
                        macro->parameters[thermal_centre_y] *= scale;
                        macro->parameters[thermal_outside_diameter] *= scale;
                        macro->parameters[thermal_inside_diameter] *= scale;
                        macro->parameters[thermal_crosshair_line_width] *= scale;
                        break;

                    case aperture_type_macro_line20:
                        exposure = macro->parameters[line_20_exposure];
                        macro->parameters[line_20_line_width] *= scale;
                        macro->parameters[line_20_start_x] *= scale;
                        macro->parameters[line_20_start_y] *= scale;
                        macro->parameters[line_20_end_x] *= scale;
                        macro->parameters[line_20_end_y] *= scale;
                        break;

                    case aperture_type_macro_line21:
                    case aperture_type_macro_line22:
                        exposure = macro->parameters[line_21_exposure];
                        macro->parameters[line_21_line_width] *= scale;
                        macro->parameters[line_21_line_height] *= scale;
                        macro->parameters[line_21_centre_x] *= scale;
                        macro->parameters[line_21_centre_y] *= scale;
                        break;

                    default:
                        break;
                    }

                    clear_operator_used = fabs(exposure) < 0.01;

                    macro_parameters_list.push_back(macro);

                    LOG_VERBOSE("MACRO: {}", macro->aperture_type);
                    int index = 1;
                    for(auto const &v : macro->parameters) {
                        LOG_VERBOSE("${}={}", index, v);
                        index += 1;
                    }
                }
                macro_stack.clear();
                break;

            default:
                break;
            }
        }
        // parameters[0] = clear_operator_used ? 1.0 : 0.0;
        LOG_DEBUG("aperture macro \"{}\" aperture macro: clear operator was {}used", aperture_macro->name, clear_operator_used ? "" : "not ");

        return ok;
    }

    //////////////////////////////////////////////////////////////////////

    std::string gerber_aperture::get_description(double scale, std::string const &units) const
    {
        switch(aperture_type) {
        case aperture_type_none: {
            return "None";
        } break;
        case aperture_type_circle: {
            return std::format("Circle, radius {:g}{}", parameters[0] / scale, units);
        } break;
        case aperture_type_rectangle: {
            return std::format("Rect, {:g}x{:g}{}", parameters[0] / scale, parameters[1] / scale, units);
        } break;
        case aperture_type_oval: {
            return std::format("Oval, {:g}x{:g}{}", parameters[0] / scale, parameters[1] / scale, units);
        } break;
        case aperture_type_polygon: {
            return std::format("Polygon");
        } break;
        case aperture_type_macro: {
            return std::format("Macro");
        } break;
        default:
            return std::format("Invalid {} ?", (int)aperture_type);
        }
    }

    //////////////////////////////////////////////////////////////////////

    gerber_aperture ::~gerber_aperture()
    {
        for(auto p : macro_parameters_list) {
            delete p;
        }
        macro_parameters_list.clear();
        parameters.clear();
    }
}    // namespace gerber_lib
