/* $Id$ */
#include <pthread.h>
#include <stdlib.h>

#include "GL/glc.h"
#include "internal.h"

FT_Library library;

static GLboolean *__glcContextIsCurrent = NULL;
static GLCenum *__glcCurrentError = NULL;
static __glcContextState **__glcContextStateList = NULL;
static pthread_once_t __glcInitThreadOnce = PTHREAD_ONCE_INIT;
static pthread_mutex_t __glcCommonAreaMutex;
static pthread_key_t __glcContextKey;
char __glcBuffer[GLC_STRING_CHUNK];

static void __glcInitThread(void)
{
    pthread_mutex_init(&__glcCommonAreaMutex, NULL);
    if (pthread_key_create(&__glcContextKey, NULL)) {
	/* Initialisation has failed. What do we do ? */
    }
    if (FT_Init_FreeType(&library)) {
	/* Initialisation has failed. What do we do ? */
    }
}

static void __glcSetCurrentError(GLint inContext, GLCenum inError)
{
    pthread_mutex_lock(&__glcCommonAreaMutex);
    __glcCurrentError[inContext] = inError;
    pthread_mutex_unlock(&__glcCommonAreaMutex);
}

__glcContextState* __glcGetCurrentState(void)
{
    return (__glcContextState *)pthread_getspecific(__glcContextKey);
}

void __glcRaiseError(GLCenum inError)
{
    __glcContextState *state = pthread_getspecific(__glcContextKey);

    if (!state)
	return;
    else
	__glcSetCurrentError(state->id - 1, inError);
}

static GLboolean __glcGetContextCurrency(GLint inContext)
{
    GLboolean isCurrent = GL_FALSE;

    pthread_mutex_lock(&__glcCommonAreaMutex);
    isCurrent = __glcContextIsCurrent[inContext];
    pthread_mutex_unlock(&__glcCommonAreaMutex);

    return isCurrent;
}

static void __glcSetContextCurrency(GLint inContext, GLboolean isCurrent)
{
    pthread_mutex_lock(&__glcCommonAreaMutex);
    __glcContextIsCurrent[inContext] = isCurrent;
    pthread_mutex_unlock(&__glcCommonAreaMutex);
}

static __glcContextState* __glcGetContextState(GLint inContext)
{
    __glcContextState *state = NULL;

    pthread_mutex_lock(&__glcCommonAreaMutex);
    state = __glcContextStateList[inContext];
    pthread_mutex_unlock(&__glcCommonAreaMutex);
    
    return state;
}

static void __glcSetContextState(GLint inContext, __glcContextState *state)
{
    pthread_mutex_lock(&__glcCommonAreaMutex);
    __glcContextStateList[inContext] = state;
    pthread_mutex_unlock(&__glcCommonAreaMutex);
}

static GLboolean __glcIsContext(GLint inContext)
{
    __glcContextState *state = NULL;
    
    pthread_mutex_lock(&__glcCommonAreaMutex);
    state = __glcContextStateList[inContext];
    pthread_mutex_unlock(&__glcCommonAreaMutex);
    
    return (state ? GL_TRUE : GL_FALSE);
}

GLboolean glcIsContext(GLint inContext)
{
    pthread_once(&__glcInitThreadOnce, __glcInitThread);

    if ((inContext < 1) || (inContext > GLC_MAX_CONTEXTS))
	return GL_FALSE;
    else
	return __glcIsContext(inContext - 1);
}

GLint glcGetCurrentContext(void)
{
    __glcContextState *state = NULL;
    
    pthread_once(&__glcInitThreadOnce, __glcInitThread);

    state = (__glcContextState *)pthread_getspecific(__glcContextKey);
    if (!state)
	return 0;
    else
	return state->id;
}

static void __glcDeleteContext(GLint inContext)
{
    __glcContextState *state = NULL;
    int i = 0;
    
    __glcSetCurrentError(inContext, GLC_NONE);
    pthread_mutex_lock(&__glcCommonAreaMutex);
    state = __glcContextStateList[inContext];
    __glcContextStateList[inContext] = NULL;
    pthread_mutex_unlock(&__glcCommonAreaMutex);

    __glcStringListDelete(&state->catalogList);
    for (i = 0; i < GLC_MAX_FONT; i++) {
	__glcFont *font = NULL;
	
	font = state->fontList[i];
	if (font) {
	    FT_Done_Face(font->face);
	    free(font);
	}
    }
    
    for (i = 0; i < GLC_MAX_MASTER; i++) {
	__glcMaster *master = NULL;
	
	master = state->masterList[i];
	if (master) {
	    __glcStringListDelete(&master->faceList);
	    __glcStringListDelete(&master->faceFileName);
	    free(master->family);
	    free(master);
	}
    }

    free(state);
}

void glcDeleteContext(GLint inContext)
{
    pthread_once(&__glcInitThreadOnce, __glcInitThread);

    /* verify parameters are in legal bounds */
    if ((inContext < 1) || (inContext > GLC_MAX_CONTEXTS)) {
	__glcRaiseError(GLC_PARAMETER_ERROR);
	return;
    }
    /* verify that the context has been created */
    if (!__glcIsContext(inContext - 1)) {
	__glcRaiseError(GLC_PARAMETER_ERROR);
	return;
    }
    
    if (__glcGetContextCurrency(inContext - 1))
	__glcGetContextState(inContext - 1)->delete = GL_TRUE;
    else
	__glcDeleteContext(inContext - 1);
}

void glcContext(GLint inContext)
{
    char *version = NULL;
    char *extension = NULL;
    
    pthread_once(&__glcInitThreadOnce, __glcInitThread);

    if (inContext) {
	GLint currentContext = 0;
	
	/* verify parameters are in legal bounds */
	if ((inContext < 0) || (inContext > GLC_MAX_CONTEXTS)) {
	    __glcRaiseError(GLC_PARAMETER_ERROR);
	    return;
	}
	/* verify that the context has been created */
	if (!__glcIsContext(inContext - 1)) {
	    __glcRaiseError(GLC_PARAMETER_ERROR);
	    return;
	}
	
	currentContext = glcGetCurrentContext();
	
	/* Is the context already current ? */
	if (__glcGetContextCurrency(inContext - 1)) {
	    /* If the context is current to another thread -> ERROR ! */
	    if (currentContext != inContext)
		__glcRaiseError(GLC_STATE_ERROR);
	    
	    /* Anyway the context is already current to one thread
	       then return */
	    return;
	}
	
	/* Release old current context if any */
	if (currentContext) {
	    __glcContextState *state = NULL;
	    
	    __glcSetContextCurrency(currentContext - 1, GL_FALSE);
	    state = __glcGetContextState(currentContext - 1);
	    
	    if (!state)
		__glcRaiseError(GLC_INTERNAL_ERROR);
	    else {
		/* execute pending deletion if any */
		if (state->delete)
		    __glcDeleteContext(currentContext - 1);
	    }
	}
	
	/* Make the context current to the thread */
	pthread_setspecific(__glcContextKey, __glcContextStateList[inContext - 1]);
	__glcSetContextCurrency(inContext - 1, GL_TRUE);
    }
    else {
	GLint currentContext = 0;
	
	currentContext = glcGetCurrentContext();
	
	if (currentContext) {
	    pthread_setspecific(__glcContextKey, NULL);
	    __glcSetContextCurrency(currentContext - 1, GL_FALSE);
	}
    }
    
    version = (char *)glGetString(GL_VERSION);
    extension = (char *)glGetString(GL_EXTENSIONS);
}

GLint glcGenContext(void)
{
    int i = 0;
    int j = 0;
    __glcContextState *state = NULL;

    pthread_once(&__glcInitThreadOnce, __glcInitThread);

    for (i=0 ; i<GLC_MAX_CONTEXTS; i++) {
	if (!__glcIsContext(i))
	    break;
    }
    
    if (i == GLC_MAX_CONTEXTS) {
	__glcRaiseError(GLC_RESOURCE_ERROR);
	return 0;
    }
    
    state = (__glcContextState *)malloc(sizeof(__glcContextState) * GLC_MAX_CONTEXTS);
    if (!state) {
	__glcRaiseError(GLC_RESOURCE_ERROR);
	return 0;
    }
    __glcSetContextState(i, state);
    __glcSetContextCurrency(i, GL_FALSE);
    __glcSetCurrentError(i, GLC_NONE);
    
    state->id = i + 1;
    state->delete = GL_FALSE;
    state->callback = GLC_NONE;
    state->dataPointer = NULL;
    state->autoFont = GL_TRUE;
    state->glObjects = GL_TRUE;
    state->mipmap = GL_TRUE;
    state->resolution = 0.;
    state->bitmapMatrix[0] = 1.;
    state->bitmapMatrix[1] = 0.;
    state->bitmapMatrix[2] = 0.;
    state->bitmapMatrix[3] = 1.;
    state->currentFontCount = 0;
    state->fontCount = 0;
    state->listObjectCount = 0;
    state->masterCount = 0;
    state->measuredCharCount = 0;
    state->renderStyle = GLC_BITMAP;
    state->replacementCode = 0;
    state->stringType = GLC_UCS1;
    state->textureObjectCount = 0;
    state->versionMajor = 0;
    state->versionMinor = 2;
    
    for (j=0; j < GLC_MAX_CURRENT_FONT; j++)
	state->currentFontList[j] = 0;

    for (j=0; j < GLC_MAX_FONT; j++)
	state->fontList[j] = NULL;

    for (j=0; j < GLC_MAX_LIST_OBJECT; j++)
	state->listObjectList[j] = 0;

    for (j=0; j < GLC_MAX_MASTER; j++)
	state->masterList[j] = NULL;

    for (j=0; j < GLC_MAX_TEXTURE_OBJECT; j++)
	state->textureObjectList[j] = 0;

    if (__glcStringListInit(&state->catalogList, state)) {
	__glcSetContextState(i, NULL);
	free(state);
	return 0;
    }
    
    return i + 1;
}

GLint* glcGetAllContexts(void)
{
    int count = 0;
    int i = 0;
    GLint* contextArray = NULL;

    pthread_once(&__glcInitThreadOnce, __glcInitThread);

    for (i=0; i < GLC_MAX_CONTEXTS; i++) {
	if (__glcIsContext(i))
	    count++;
    }
    
    contextArray = (GLint *)malloc(sizeof(GLint) * count);
    if (!contextArray) {
	__glcRaiseError(GLC_RESOURCE_ERROR);
	return NULL;
    }

    for (i=0; i < GLC_MAX_CONTEXTS; i++) {
	if (__glcIsContext(i))
	    contextArray[--count] = i;
    }
    
    return contextArray;
}

GLCenum glcGetError(void)
{
    GLCenum error = GLC_NONE;
    __glcContextState *state = NULL;
    
    pthread_once(&__glcInitThreadOnce, __glcInitThread);

    state = (__glcContextState *)pthread_getspecific(__glcContextKey);
    if (state) {
	error = __glcCurrentError[state->id - 1];
	__glcSetCurrentError(state->id - 1, GLC_NONE);
    }
    
    return error;
}

void my_init(void)
{
    int i = 0;
   
    /* create Common Area */
    __glcContextIsCurrent = (GLboolean *)malloc(sizeof(GLboolean) * GLC_MAX_CONTEXTS);
    if (!__glcContextIsCurrent)
	return;

    __glcCurrentError = (GLCenum *)malloc(sizeof(GLCenum) * GLC_MAX_CONTEXTS);
    if (!__glcCurrentError) {
	free(__glcContextIsCurrent);
	return;
    }
    
    __glcContextStateList = (__glcContextState **)malloc(sizeof(__glcContextState*) * GLC_MAX_CONTEXTS);
    if (!__glcContextStateList) {
	free(__glcContextIsCurrent);
	free(__glcCurrentError);
	return;
    }
   
    for (i=0; i< GLC_MAX_CONTEXTS; i++) {
	__glcContextIsCurrent[i] = GL_FALSE;
	__glcCurrentError[i] = GLC_NONE;
	__glcContextStateList[i] = NULL;
    }
}

void my_fini(void)
{
	int i;
	
	/* destroy remaining contexts */
	for (i=0; i < GLC_MAX_CONTEXTS; i++) {
	    if (__glcIsContext(i))
		__glcDeleteContext(i);
	}
	
	/* destroy Common Area */
	free(__glcContextIsCurrent);
	free(__glcCurrentError);
	free(__glcContextStateList);
}
