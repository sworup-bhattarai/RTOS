; mspTopsp.s


	.def setASPbit
	.def setPSP

.thumb

.text


setASPbit:
	MRS	R0, CONTROL
	ORR	R0, #2
	MSR	CONTROL, R0
	ISB
	BX	LR

setPSP:
	MRS	R0, PSP
	BX	LR

getMSP:
	MRS	R0, MSP
	BX	LR

getPSP:
	MRS	R0, PSP
	BX	LR

.endm
