/* $Id$ */
#include <stdlib.h>
#include <string.h>

#include "GL/glc.h"
#include "internal.h"

void glcCallbackFunc(GLCenum inOpcode, GLCfunc inFunc)
{
    __glcContextState *state = NULL;
    
    if (inOpcode != GLC_OP_glcUnmappedCode) {
	__glcRaiseError(GLC_PARAMETER_ERROR);
	return;
    }
    
    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return;
    }
    
    state->callback = inFunc;
}

void glcDataPointer(GLvoid *inPointer)
{
    __glcContextState *state = NULL;

    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return;
    }

    state->dataPointer = inPointer;
}

static void __glcDisable(GLCenum inAttrib, GLboolean value)
{
    __glcContextState *state = NULL;

    switch(inAttrib) {
	case GLC_AUTO_FONT:
	case GLC_GL_OBJECTS:
	case GLC_MIPMAP:
	    break;
	default:
	    __glcRaiseError(GLC_PARAMETER_ERROR);
	    return;
    }
    
    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return;
    }

    switch(inAttrib) {
	case GLC_AUTO_FONT:
	    state->autoFont = value;
	    break;
	case GLC_GL_OBJECTS:
	    state->glObjects = value;
	    break;
	case GLC_MIPMAP:
	    state->mipmap = value;
    }
}

void glcDisable(GLCenum inAttrib)
{
    __glcDisable(inAttrib, GL_FALSE);
}

void glcEnable(GLCenum inAttrib)
{
    __glcDisable(inAttrib, GL_TRUE);
}

GLCfunc glcGetCallbackFunc(GLCenum inOpcode)
{
    __glcContextState *state = NULL;

    if (inOpcode != GLC_OP_glcUnmappedCode) {
	__glcRaiseError(GLC_PARAMETER_ERROR);
	return GLC_NONE;
    }
    
    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return GLC_NONE;
    }

    return state->callback;
}

const GLCchar* glcGetListc(GLCenum inAttrib, GLint inIndex)
{
    __glcContextState *state = NULL;

    if (inAttrib != GLC_CATALOG_LIST) {
	__glcRaiseError(GLC_PARAMETER_ERROR);
	return GLC_NONE;
    }
    
    if (inIndex < 0) {
	__glcRaiseError(GLC_PARAMETER_ERROR);
	return GLC_NONE;
    }
    
    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return GLC_NONE;
    }

    if ((inIndex < 0) || (inIndex >= state->catalogList.count)) {
	__glcRaiseError(GLC_PARAMETER_ERROR);
	return GLC_NONE;
    }
    
    return __glcStringListExtractElement(&state->catalogList, inIndex, (GLCchar *)__glcBuffer, GLC_STRING_CHUNK);
}

GLint glcGetListi(GLCenum inAttrib, GLint inIndex)
{
    __glcContextState *state = NULL;

    switch(inAttrib) {
	case GLC_CURRENT_FONT_LIST:
	case GLC_FONT_LIST:
	case GLC_LIST_OBJECT_LIST:
	case GLC_TEXTURE_OBJECT_LIST:
	    break;
	default:
	    __glcRaiseError(GLC_PARAMETER_ERROR);
	    return 0;
    }
    
    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return 0;
    }

    switch(inAttrib) {
	case GLC_CURRENT_FONT_LIST:
	    if (inIndex > state->currentFontCount) {
		__glcRaiseError(GLC_PARAMETER_ERROR);
		return 0;
	    }
	    return state->currentFontList[inIndex];
	case GLC_FONT_LIST:
	    if (inIndex > state->fontCount) {
		__glcRaiseError(GLC_PARAMETER_ERROR);
		return 0;
	    }
	    return (state->fontList[inIndex] ? inIndex : 0);
	case GLC_LIST_OBJECT_LIST:
	    if (inIndex > state->listObjectCount) {
		__glcRaiseError(GLC_PARAMETER_ERROR);
		return 0;
	    }
	    return state->listObjectList[inIndex];
	case GLC_TEXTURE_OBJECT_LIST:
	    if (inIndex > state->textureObjectCount) {
		__glcRaiseError(GLC_PARAMETER_ERROR);
		return 0;
	    }
	    return state->textureObjectList[inIndex];
    }
    
    return 0;
}

GLvoid * glcGetPointer(GLCenum inAttrib)
{
    __glcContextState *state = NULL;

    if (inAttrib != GLC_DATA_POINTER) {
	__glcRaiseError(GLC_PARAMETER_ERROR);
	return GLC_NONE;
    }
    
    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return GLC_NONE;
    }

    return state->dataPointer;
}

const GLCchar* glcGetc(GLCenum inAttrib)
{
    __glcContextState *state = NULL;
    static const GLCchar* __glcExtensions = "";
    static const GLCchar* __glcRelease = "alpha1 Linux";
    static const GLCchar* __glcVendor = "GNU";

    switch(inAttrib) {
	case GLC_EXTENSIONS:
	case GLC_RELEASE:
	case GLC_VENDOR:
	    break;
	default:
	    __glcRaiseError(GLC_PARAMETER_ERROR);
	    return GLC_NONE;
    }

    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return GLC_NONE;
    }

    switch(inAttrib) {
	case GLC_EXTENSIONS:
	    return __glcExtensions;
	case GLC_RELEASE:
	    return __glcRelease;
	case GLC_VENDOR:
	    return __glcVendor;
    }
    
    return GLC_NONE;
}

GLfloat glcGetf(GLCenum inAttrib)
{
    __glcContextState *state = NULL;

    if (inAttrib != GLC_RESOLUTION) {
	__glcRaiseError(GLC_PARAMETER_ERROR);
	return 0.;
    }
    
    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return 0.;
    }

    return state->resolution;
}

GLfloat* glcGetfv(GLCenum inAttrib, GLfloat* outVec)
{
    __glcContextState *state = NULL;

    if (inAttrib != GLC_BITMAP_MATRIX) {
	__glcRaiseError(GLC_PARAMETER_ERROR);
	return NULL;
    }
    
    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return NULL;
    }

    memcpy(outVec, state->bitmapMatrix, 4 * sizeof(GLfloat));
    
    return outVec;
}

GLint glcGeti(GLCenum inAttrib)
{
    __glcContextState *state = NULL;

    switch(inAttrib) {
	case GLC_CATALOG_COUNT:
	case GLC_CURRENT_FONT_COUNT:
	case GLC_FONT_COUNT:
	case GLC_LIST_OBJECT_COUNT:
	case GLC_MASTER_COUNT:
	case GLC_MEASURED_CHAR_COUNT:
	case GLC_RENDER_STYLE:
	case GLC_REPLACEMENT_CODE:
	case GLC_STRING_TYPE:
	case GLC_TEXTURE_OBJECT_COUNT:
	case GLC_VERSION_MAJOR:
	case GLC_VERSION_MINOR:
	    break;
	default:
	    __glcRaiseError(GLC_PARAMETER_ERROR);
	    return GLC_NONE;
    }

    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return 0;
    }

    switch(inAttrib) {
	case GLC_CATALOG_COUNT:
	    return state->catalogList.count;
	case GLC_CURRENT_FONT_COUNT:
	    return state->currentFontCount;
	case GLC_FONT_COUNT:
	    return state->fontCount;
	case GLC_LIST_OBJECT_COUNT:
	    return state->listObjectCount;
	case GLC_MASTER_COUNT:
	    return state->masterCount;
	case GLC_MEASURED_CHAR_COUNT:
	    return state->measuredCharCount;
	case GLC_RENDER_STYLE:
	    return state->renderStyle;
	case GLC_REPLACEMENT_CODE:
	    return state->replacementCode;
	case GLC_STRING_TYPE:
	    return state->stringType;
	case GLC_TEXTURE_OBJECT_COUNT:
	    return state->textureObjectCount;
	case GLC_VERSION_MAJOR:
	    return state->versionMajor;
	case GLC_VERSION_MINOR:
	    return state->versionMinor;
    }
    
    return 0;
}

GLboolean glcIsEnabled(GLCenum inAttrib)
{
    __glcContextState *state = NULL;

    switch(inAttrib) {
	case GLC_AUTO_FONT:
	case GLC_GL_OBJECTS:
	case GLC_MIPMAP:
	    break;
	default:
	    __glcRaiseError(GLC_PARAMETER_ERROR);
	    return GL_FALSE;
    }
    
    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return GL_FALSE;
    }

    switch(inAttrib) {
	case GLC_AUTO_FONT:
	    return state->autoFont;
	case GLC_GL_OBJECTS:
	    return state->glObjects;
	case GLC_MIPMAP:
	    return state->mipmap;
    }
    
    return GL_FALSE;
}

void glcStringType(GLCenum inStringType)
{
    __glcContextState *state = NULL;

    switch(inStringType) {
	case GLC_UCS1:
	case GLC_UCS2:
	case GLC_UCS4:
	    break;
	default:
	    __glcRaiseError(GLC_PARAMETER_ERROR);
	    return;
    }
    
    state = __glcGetCurrentState();
    if (!state) {
	__glcRaiseError(GLC_STATE_ERROR);
	return;
    }

    state->stringType = inStringType;
    return;
}

/* TODO This funcs are not implemented */
void glcDeleteGLObjects(void)
{
    return;
}
