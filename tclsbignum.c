/* Tcl bindings for sbignum.c
 * Copyright (C) 2003 Salvatore Sanfilippo <antirez@invece.org>
 * All rights reserved.
 *
 * See LICENSE for Copyright and License information. */

#include <tcl.h>
#include "sbignum.h"

#define VERSION "0.1"

/* -------------------------- Mpz object implementation --------------------- */

static void Tcl_SetMpzObj(Tcl_Obj *objPtr, mpz_ptr val);
//static Tcl_Obj *Tcl_NewMpzObj(void);
static void FreeMpzInternalRep(Tcl_Obj *objPtr);
static void DupMpzInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static void UpdateStringOfMpz(Tcl_Obj *objPtr);
static int SetMpzFromAny(struct Tcl_Interp* interp, Tcl_Obj *objPtr);

struct Tcl_ObjType tclMpzType = {
	"mpz",
	FreeMpzInternalRep,
	DupMpzInternalRep,
	UpdateStringOfMpz,
	SetMpzFromAny
};

/* This function set objPtr as an mpz object with value
 * 'val'. If 'val' == NULL, the mpz object is set to zero. */
void Tcl_SetMpzObj(Tcl_Obj *objPtr, mpz_ptr val)
{
	Tcl_ObjType *typePtr;
	mpz_ptr mpzPtr;

	/* It's not a good idea to set a shared object... */
	if (Tcl_IsShared(objPtr)) {
		panic("Tcl_SetMpzObj called with shared object");
	}
	/* Free the old object private data and invalidate the string
	 * representation. */
	typePtr = objPtr->typePtr;
	if ((typePtr != NULL) && (typePtr->freeIntRepProc != NULL)) {
		(*typePtr->freeIntRepProc)(objPtr);
	}
	Tcl_InvalidateStringRep(objPtr);
	/* Allocate and initialize a new bignum */
	mpzPtr = (mpz_ptr) ckalloc(sizeof(struct struct_sbnz));
	mpz_init(mpzPtr);
	if (val && mpz_set(mpzPtr, val) != SBN_OK) {
		panic("Out of memory in Tcl_SetMpzObj");
	}
	/* Set it as object private data, and type */
	objPtr->typePtr = &tclMpzType;
	objPtr->internalRep.otherValuePtr = (void*) mpzPtr;
}

/* Return an mpz from the object. If the object is not of type mpz
 * an attempt to convert it to mpz is done. On failure (the string
 * representation of the object can't be converted on a bignum)
 * an error is returned. */
int Tcl_GetMpzFromObj(struct Tcl_Interp *interp, Tcl_Obj *objPtr, mpz_ptr *mpzPtrPtr)
{
	int result;

	if (objPtr->typePtr != &tclMpzType) {
		result = SetMpzFromAny(interp, objPtr);
		if (result != TCL_OK)
			return result;
	}
	*mpzPtrPtr = (mpz_ptr) objPtr->internalRep.longValue;
	return TCL_OK;
}

/* Create a new mpz object */
Tcl_Obj *Tcl_NewMpzObj(void)
{
	struct Tcl_Obj *objPtr;

	/* Create a new Tcl Object */
	objPtr = Tcl_NewObj();
	Tcl_SetMpzObj(objPtr, 0);
	return objPtr;
}

/* The 'free' method of the object. */
void FreeMpzInternalRep(Tcl_Obj *objPtr)
{
	mpz_ptr mpzPtr = (mpz_ptr) objPtr->internalRep.otherValuePtr;

	mpz_clear(mpzPtr);
	ckfree((void*)mpzPtr);
}

/* The 'dup' method of the object */
void DupMpzInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr)
{
	mpz_ptr mpzCopyPtr = (mpz_ptr) ckalloc(sizeof(struct struct_sbnz));
	mpz_ptr mpzSrcPtr;

	mpz_init(mpzCopyPtr);
	mpzSrcPtr = (mpz_ptr) srcPtr->internalRep.otherValuePtr;
	if (mpz_set(mpzCopyPtr, mpzSrcPtr) != SBN_OK)
		panic("Out of memory inside DupMpzInternalRep()");
	copyPtr->internalRep.otherValuePtr = (void*) mpzCopyPtr;
	copyPtr->typePtr = &tclMpzType;
}

/* The 'update string' method of the object */
void UpdateStringOfMpz(Tcl_Obj *objPtr)
{
	size_t len;
	mpz_ptr mpzPtr = (mpz_ptr) objPtr->internalRep.otherValuePtr;

	len = mpz_sizeinbase(mpzPtr, 10)+2;
	objPtr->bytes = ckalloc(len);
	mpz_get_str(objPtr->bytes, 10, mpzPtr);
	/* XXX: fixme, modifing the sbignum library it is
	 * possible to get the length of the written string. */
	objPtr->length = strlen(objPtr->bytes);
}

/* The 'set from any' method of the object */
int SetMpzFromAny(struct Tcl_Interp* interp, Tcl_Obj *objPtr)
{
	char *s;
	mpz_t t;
	mpz_ptr mpzPtr;
	Tcl_ObjType *typePtr;

	if (objPtr->typePtr == &tclMpzType)
		return TCL_OK;

	/* Try to convert */
	s = Tcl_GetStringFromObj(objPtr, NULL);
	mpz_init(t);
	if (mpz_set_str(t, s, 0) != SBN_OK) {
		mpz_clear(t);
		Tcl_ResetResult(interp);
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
				"Invalid big number: \"",
				s, "\" must be a relative integer number",
				NULL);
		return TCL_ERROR;
	}
	/* Allocate */
	mpzPtr = (mpz_ptr) ckalloc(sizeof(struct struct_sbnz));
	mpz_init(mpzPtr);
	/* Free the old object private rep */
	typePtr = objPtr->typePtr;
	if ((typePtr != NULL) && (typePtr->freeIntRepProc != NULL)) {
		(*typePtr->freeIntRepProc)(objPtr);
	}
	/* Set it */
	objPtr->typePtr = &tclMpzType;
	objPtr->internalRep.otherValuePtr = (void*) mpzPtr;
	memcpy(mpzPtr, t, sizeof(*mpzPtr));
	return TCL_OK;
}

/* --------------- the actual commands for multipreicision math ------------- */

static int BigBasicObjCmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *result;
	mpz_t res;
	mpz_ptr t;
	char *cmd;

	cmd = Tcl_GetStringFromObj(objv[0], NULL);
	objc--;
	objv++;

	result = Tcl_GetObjResult(interp);
	mpz_init(res);
	mpz_setzero(res);
	if (cmd[0] == '*' || cmd[0] == '/') {
		if (mpz_set_ui(res, 1) != SBN_OK)
			goto err;
	}
	if ((cmd[0] == '/' || cmd[0] == '%' || cmd[0] == '-') && objc) {
		if (Tcl_GetMpzFromObj(interp, objv[0], &t) != TCL_OK)
			goto err;
		if (mpz_set(res, t) != SBN_OK)
			goto oom;
		if (cmd[0] == '-' && objc == 1)
			res->s = !res->s;
		objc--;
		objv++;
	}
	while(objc--) {
		if (Tcl_GetMpzFromObj(interp, objv[0], &t) != TCL_OK)
			goto err;
		switch(cmd[0]) {
		case '+':
			if (mpz_add(res, res, t) != SBN_OK)
				goto oom;
			break;
		case '-':
			if (mpz_sub(res, res, t) != SBN_OK)
				goto oom;
			break;
		case '*':
			if (mpz_mul(res, res, t) != SBN_OK)
				goto oom;
			break;
		case '/':
			if (mpz_tdiv_q(res, res, t) != SBN_OK)
				goto oom;
			break;
		case '%':
			if (mpz_mod(res, res, t) != SBN_OK)
				goto oom;
			break;
		}
		objv++;
	}
	Tcl_SetMpzObj(result, res);
	mpz_clear(res);
	return TCL_OK;
err:
	mpz_clear(res);
	return TCL_ERROR;
oom:
	Tcl_SetStringObj(result, "Out of memory doing multiprecision math", -1);
	mpz_clear(res);
	return TCL_ERROR;
}

static int BigCmpObjCmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *result;
	mpz_ptr a, b;
	int cmp, res;
	char *cmd;

	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "bignum bignum");
		return TCL_ERROR;
	}

	cmd = Tcl_GetStringFromObj(objv[0], NULL);
	if (Tcl_GetMpzFromObj(interp, objv[1], &a) != TCL_OK ||
	    Tcl_GetMpzFromObj(interp, objv[2], &b) != TCL_OK)
		return TCL_ERROR;
	cmp = mpz_cmp(a, b);

	result = Tcl_GetObjResult(interp);
	res = 0;
	switch(cmd[0]) {
	case '>':
		switch(cmd[1]) {
		case '=':
			if (cmp >= 0) res = 1;
			break;
		default:
			if (cmp > 0) res = 1;
			break;
		}
		break;
	case '<':
		switch(cmd[1]) {
		case '=':
			if (cmp <= 0) res = 1;
			break;
		default:
			if (cmp < 0) res = 1;
			break;
		}
		break;
	case '=':
		if (cmp == 0) res = 1;
		break;
	case '!':
		if (cmp != 0) res = 1;
		break;
	}
	Tcl_SetIntObj(result, res);
	return TCL_OK;
}

static int BigRandObjCmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *result;
	int len = 1;
	mpz_t r;

	if (objc != 1 && objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "?atoms?");
		return TCL_ERROR;
	}
	if (objc == 2 && Tcl_GetIntFromObj(interp, objv[1], &len) != TCL_OK)
		return TCL_ERROR;
	result = Tcl_GetObjResult(interp);
	mpz_init(r);
	if (mpz_random(r, len) != SBN_OK) {
		mpz_clear(r);
		Tcl_SetStringObj(result, "Out of memory", -1);
		return TCL_ERROR;
	}
	Tcl_SetMpzObj(result, r);
	mpz_clear(r);
	return TCL_OK;
}

static int BigSrandObjCmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
	char *seed;
	int len;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "seed-string");
		return TCL_ERROR;
	}
	seed = Tcl_GetStringFromObj(objv[1], &len);
	sbn_seed(seed, len);
	return TCL_OK;
}

static int BigPowObjCmd(ClientData clientData, Tcl_Interp *interp,
		int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *result;
	int mpzerr;
	mpz_t r; /* result */
	mpz_ptr b, e, m; /* base, exponent, modulo */

	if (objc != 3 && objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "base exponent ?modulo?");
		return TCL_ERROR;
	}
	if (Tcl_GetMpzFromObj(interp, objv[1], &b) != TCL_OK ||
	    Tcl_GetMpzFromObj(interp, objv[2], &e) != TCL_OK ||
	    (objc == 4 && Tcl_GetMpzFromObj(interp, objv[3], &m) != TCL_OK))
		return TCL_ERROR;
	result = Tcl_GetObjResult(interp);
	mpz_init(r);
	if (objc == 4)
		mpzerr = mpz_powm(r, b, e, m);
	else
		mpzerr = mpz_pow(r, b, e);
	if (mpzerr != SBN_OK) {
		mpz_clear(r);
		if (mpzerr == SBN_INVAL)
			Tcl_SetStringObj(result, "Negative exponent", -1);
		else
			Tcl_SetStringObj(result, "Out of memory", -1);
		return TCL_ERROR;
	}
	Tcl_SetMpzObj(result, r);
	mpz_clear(r);
	return TCL_OK;
}

/* -------------------------------  Initialization -------------------------- */
int Tclsbignum_Init(Tcl_Interp *interp)
{
	if (Tcl_InitStubs(interp, "8.0", 0) == NULL)
		return TCL_ERROR;
	if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 0) == NULL)
		return TCL_ERROR;
	if (Tcl_PkgProvide(interp, "tclsbignum", VERSION) != TCL_OK)
		return TCL_ERROR;
	Tcl_CreateObjCommand(interp, "+", BigBasicObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, "-", BigBasicObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, "*", BigBasicObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, "/", BigBasicObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, "%", BigBasicObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, ">", BigCmpObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, ">=", BigCmpObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, "<", BigCmpObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, "<=", BigCmpObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, "==", BigCmpObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, "!=", BigCmpObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, "rand", BigRandObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, "srand", BigSrandObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	Tcl_CreateObjCommand(interp, "**", BigPowObjCmd, (ClientData)NULL,
			(Tcl_CmdDeleteProc*)NULL);
	/* Private data initialization here */
	return TCL_OK;
}
