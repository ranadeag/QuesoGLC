/* QuesoGLC
 * A free implementation of the OpenGL Character Renderer (GLC)
 * Copyright (c) 2002-2005, Bertrand Coconnier
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
/* $Id$ */

/* Defines the methods of an object that is intended to managed fonts */

#include <assert.h>

#include "internal.h"
#include "ocontext.h"
#include "ofont.h"

__glcFont* __glcFontCreate(GLint inID, __glcMaster *inParent)
{
  GLCchar *buffer = NULL;
  __glcFont *This = NULL;
  __glcContextState *state = __glcGetCurrent();

  assert(inParent);

  if (!state) {
    __glcRaiseError(GLC_STATE_ERROR);
    return NULL;
  }

  /* At font creation, the default face is the first one.
   * glcFontFace() can change the face.
   */
  This = (__glcFont*)__glcMalloc(sizeof(__glcFont));

  This->faceDesc = NULL;
  This->parent = inParent;
  This->charMapCount = 0;
  This->id = inID;

  if (FT_New_Face(state->library, 
		  (const char*)((__glcFaceDescriptor*)inParent->faceList->head->data)->fileName, 0, &This->face)) {
    /* Unable to load the face file, however this should not happen since
       it has been succesfully loaded when the master was created */
    __glcRaiseError(GLC_RESOURCE_ERROR);
    __glcFree(buffer);
    __glcFree(This);
    return NULL;
  }

  /* select a Unicode charmap */
  if (FT_Select_Charmap(This->face, ft_encoding_unicode)) {
    /* Arrghhh, no Unicode charmap is available. This should not happen
       since it has been tested at master creation */
    __glcRaiseError(GLC_RESOURCE_ERROR);
    FT_Done_Face(This->face);
    __glcFree(This);
    return NULL;
  }

  return This;
}

void __glcFontDestroy(__glcFont *This)
{
  FT_Done_Face(This->face);
  __glcFree(This);
}
