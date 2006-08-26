/* QuesoGLC
 * A free implementation of the OpenGL Character Renderer (GLC)
 * Copyright (c) 2002, 2004-2006, Bertrand Coconnier
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

#include "internal.h"

#define GLC_ARRAY_BLOCK_SIZE 16


/* \file
 * defines the object __glcArray which is an array which size can grow as some
 * new elements are added to it.
 */

/* This object heavily uses the realloc() which means that it must not be
 * assumed that the data are always stored at the same address. The safer way
 * to handle that is to *always* assume the address of the data has changed
 * *every* time a method of __glcArray is called ; whatever the method is.
 */

/* Constructor of the object : it allocates memory and initializes the member
 * of the new object.
 * The user must give the size of an element of the array.
 */
__glcArray* __glcArrayCreate(int inElementSize)
{
  __glcArray* This = NULL;

  This = (__glcArray*)__glcMalloc(sizeof(__glcArray));
  if (!This) {
    __glcRaiseError(GLC_RESOURCE_ERROR);
    return NULL;
  }

  This->data = (char*)__glcMalloc(GLC_ARRAY_BLOCK_SIZE * inElementSize);
  if (!This->data) {
    __glcFree(This);
    __glcRaiseError(GLC_RESOURCE_ERROR);
    return NULL;
  }

  This->allocated = GLC_ARRAY_BLOCK_SIZE;
  This->length = 0;
  This->elementSize = inElementSize;

  return This;
}



/* Destructor of the object */
void __glcArrayDestroy(__glcArray* This)
{
  if (This->data) {
    assert(This->allocated);
    __glcFree(This->data);
  }

  __glcFree(This);
}



/* Allocate a new block of elements in the array 'This'. The function returns
 * NULL if it fails and raises an error accordingly. However the original
 * array is not lost and is kept untouched.
 */
static __glcArray* __glcArrayUpdateSize(__glcArray* This)
{
  char* data = NULL;

  data = (char*)__glcRealloc(This->data,
	(This->allocated + GLC_ARRAY_BLOCK_SIZE) * This->elementSize);
  if (!data) {
    __glcRaiseError(GLC_RESOURCE_ERROR);
    return NULL;
  }
  This->data = data;
  This->allocated += GLC_ARRAY_BLOCK_SIZE;

  return This;
}



/* Append a value to the array. The function may allocate some more room if
 * necessary
 */
__glcArray* __glcArrayAppend(__glcArray* This, void* inValue)
{
  /* Update the room if needed */
  if (This->length == This->allocated) {
    if (!__glcArrayUpdateSize(This))
      return NULL;
  }

  /* Append the new element */
  memcpy(This->data + This->length*This->elementSize, inValue,
	 This->elementSize);
  This->length++;

  return This;
}



/* Insert a value in the array at the rank inRank. The function may allocate
 * some more room if necessary
 */
__glcArray* __glcArrayInsert(__glcArray* This, int inRank, void* inValue)
{
  /* Update the room if needed */
  if (This->length == This->allocated) {
    if (!__glcArrayUpdateSize(This))
      return NULL;
  }

  /* Insert the new element */
  if (This->length > inRank)
    memmove(This->data + (inRank+1) * This->elementSize,
	   This->data + inRank * This->elementSize,
	   (This->length - inRank) * This->elementSize);

  memcpy(This->data + inRank*This->elementSize, inValue, This->elementSize);
  This->length++;

  return This;
}



/* Remove an element from the array. For permformance reasons, this function
 * does not release memory.
 */
void __glcArrayRemove(__glcArray* This, int inRank)
{
  if (inRank < This->length-1)
    memmove(This->data + inRank * This->elementSize,
	    This->data + (inRank+1) * This->elementSize,
	    (This->length - inRank - 1) * This->elementSize);
  This->length--;
}



/* Insert some room in the array at rank 'inRank' and leave it as is.
 * The difference between __glcArrayInsertCell() and __glcArrayInsert() is that
 * __glcArrayInsert() copy a value in the new element array while
 * __glcArrayInsertCell() does not.
 * This function is used to optimize performance in certain configurations.
 */
char* __glcArrayInsertCell(__glcArray* This, int inRank)
{
  char* newCell = NULL;

  if (This->length == This->allocated) {
    if (!__glcArrayUpdateSize(This))
      return NULL;
  }

  newCell = This->data + inRank * This->elementSize;

  if (This->length > inRank)
    memmove(newCell + This->elementSize, newCell,
	   (This->length - inRank) * This->elementSize);

  This->length++;

  return newCell;
}
