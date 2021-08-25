#pragma once
#include <asm-generic/int-ll64.h>
#include "main.h"

// See Table 24-8. Format of Extended-Page-Table Pointer
typedef union _EPTP
{
	__u64 all;
	struct
	{
		__u64 MemoryType : 3; // bit 2:0 (0 = Uncacheable (UC) - 6 = Write - back(WB))
		__u64 PageWalkLength : 3; // bit 5:3 (This value is 1 less than the EPT page-walk length)
		__u64 DirtyAndAceessEnabled : 1; // bit 6  (Setting this control to 1 enables accessed and dirty flags for EPT)
		__u64 Reserved1 : 5; // bit 11:7
		__u64 PML4Address : 36;
		__u64 Reserved2 : 16;
	} fields;
} EPTP, *PEPTP;




// See Table 28-1.
typedef union _EPT_PML4E
{
	__u64 all;
	struct
	{
		__u64 Read : 1; // bit 0
		__u64 Write : 1; // bit 1
		__u64 Execute : 1; // bit 2
		__u64 Reserved1 : 5; // bit 7:3 (Must be Zero)
		__u64 Accessed : 1; // bit 8
		__u64 Ignored1 : 1; // bit 9
		__u64 ExecuteForUserMode : 1; // bit 10
		__u64 Ignored2 : 1; // bit 11
		__u64 PhysicalAddress : 36; // bit (N-1):12 or Page-Frame-Number
		__u64 Reserved2 : 4; // bit 51:N
		__u64 Ignored3 : 12; // bit 63:52
	} fields;
} EPT_PML4E, *PEPT_PML4E;



// See Table 28-3
typedef union _EPT_PDPTE
{
	__u64 all;
	struct
	{
		__u64 Read : 1; // bit 0
		__u64 Write : 1; // bit 1
		__u64 Execute : 1; // bit 2
		__u64 Reserved1 : 4; // bit 6:3 (Must be Zero)
		__u64 GiantPage : 1; // bit 7
		__u64 Accessed : 1; // bit 8
		__u64 Ignored1 : 1; // bit 9
		__u64 ExecuteForUserMode : 1; // bit 10
		__u64 Ignored2 : 1; // bit 11
		__u64 PhysicalAddress : 36; // bit (N-1):12 or Page-Frame-Number
		__u64 Reserved2 : 4; // bit 51:N
		__u64 Ignored3 : 12; // bit 63:52
	} fields;
} EPT_PDPTE, *PEPT_PDPTE;


// See Table 28-5
typedef union _EPT_PDE
{
	__u64 all;
	struct
	{
		__u64 Read : 1; // bit 0
		__u64 Write : 1; // bit 1
		__u64 Execute : 1; // bit 2
		__u64 Reserved1 : 4; // bit 6:3 (Must be Zero)
		__u64 LargePage : 1; // bit 7
		__u64 Accessed : 1; // bit 8
		__u64 Ignored1 : 1; // bit 9
		__u64 ExecuteForUserMode : 1; // bit 10
		__u64 Ignored2 : 1; // bit 11
		__u64 PhysicalAddress : 36; // bit (N-1):12 or Page-Frame-Number
		__u64 Reserved2 : 4; // bit 51:N
		__u64 Ignored3 : 12; // bit 63:52
	} fields;
} EPT_PDE, *PEPT_PDE;



// See Table 28-6
typedef union _EPT_PTE
{
	__u64 all;
	struct
	{
		__u64 Read : 1; // bit 0
		__u64 Write : 1; // bit 1
		__u64 Execute : 1; // bit 2
		__u64 EPTMemoryType : 3; // bit 5:3 (EPT Memory type)
		__u64 IgnorePAT : 1; // bit 6
		__u64 Ignored1 : 1; // bit 7
		__u64 AccessedFlag : 1; // bit 8
		__u64 DirtyFlag : 1; // bit 9
		__u64 ExecuteForUserMode : 1; // bit 10
		__u64 Ignored2 : 1; // bit 11
		__u64 PhysicalAddress : 36; // bit (N-1):12 or Page-Frame-Number
		__u64 Reserved : 4; // bit 51:N
		__u64 Ignored3 : 11; // bit 62:52
		__u64 SuppressVE : 1; // bit 63
	} fields;
} EPT_PTE, *PEPT_PTE;

