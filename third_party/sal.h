#ifndef SAL_H_SHIM
#define SAL_H_SHIM

// SAL 1 / Legacy Macros
#define __in
#define __out
#define __in_opt
#define __out_opt
#define __inout
#define __inout_opt
#define __in_ecount(x)
#define __out_ecount(x)
#define __in_bcount(x)
#define __out_bcount(x)
#define __deref_out
#define __deref_inout
#define __checkReturn

// SAL 2 Macros
#define _In_
#define _Out_
#define _Inout_
#define _In_z_
#define _In_opt_
#define _Out_opt_
#define _Inout_opt_
#define _In_reads_(s)
#define _In_reads_bytes_(s)
#define _In_reads_opt_(s)
#define _Out_writes_(s)
#define _Out_writes_bytes_(s)
#define _Out_writes_opt_(s)
#define _Out_writes_to_(s,c)
#define _Inout_updates_(s)
#define _Inout_updates_bytes_(s)
#define _Use_decl_annotations_
#define _Analysis_assume_(x)
#define _Success_(x)
#define _Return_type_success_(x)
#define _On_failure_(x)

#endif // SAL_H_SHIM