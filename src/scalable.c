/* QuesoGLC
 * A free implementation of the OpenGL Character Renderer (GLC)
 * Copyright (c) 2002-2004, Bertrand Coconnier
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

/* Draws characters of scalable fonts */

#include <GL/glu.h>
#include <stdlib.h>
#include <stdio.h>

#include "GL/glc.h"
#include "internal.h"
#include FT_OUTLINE_H
#include FT_LIST_H

#define GLC_MAX_VERTEX	1024

typedef struct __Node__ {
    GLfloat x;
    GLfloat y;
    GLfloat t;
    struct __Node__ *next;
} __glcNode;

typedef struct {
    GLUtesselator *tess;	/* GLU tesselator */
    FT_Vector pen;		/* Current coordinates */
    GLfloat tolerance;		/* Chordal tolerance */
    GLdouble (*vertex)[3];	/* Vertices array */
    GLint numVertex;		/* Number of vertices in the vertices array */
    GLfloat scale;		/* Scale to convert grid point coordinates into
				   pixel coordinates */
}__glcRendererData;

/* __glcdeCasteljau : 
 *	renders bezier curves using the de Casteljau subdivision algorithm
 *
 * This function creates a piecewise linear curve which is close enough
 * to the real Bezier curve. The piecewise linear curve is built so that
 * the chordal distance is lower than a tolerance value.
 * The chordal distance is taken to be the perpendicular distance from the
 * parametric midpoint, (t = 0.5), to the chord. This may not always be correct,
 * but, in the small lengths which are being considered, this is good enough.
 * In order to mitigate any error, the chordal tolerance is taken to be slightly
 * smaller than the tolerance specified by the user. 
 * A second simplifying assumption is that when too large a tolerance is
 * encountered, the chord is split at the parametric midpoint, rather than
 * guessing the exact location of the best chord. This could lead to slightly
 * sub-optimal lines, but it provides a fast method for choosing the subdivision
 * point. This guess can be refined by lengthening the lines.
 */ 
static void __glcdeCasteljau(FT_Vector *inVecTo, FT_Vector **inControl, void *inUserData, GLint inOrder)
{
    GLboolean loop = GL_TRUE;
    __glcRendererData *data = (__glcRendererData *) inUserData;
    GLfloat px[10], py[10];
    __glcNode nodeList[256];		/* FIXME : use a dynamic list */
    __glcNode *firstNode = &nodeList[0];
    __glcNode *currentNode = &nodeList[1];
    __glcNode *endList = &nodeList[2];
    GLint i = 0;
    
    px[0] = (GLfloat) data->pen.x;
    py[0] = (GLfloat) data->pen.y;
    for (i = 1; i < inOrder; i++) {
	px[i] = (GLfloat) inControl[i - 1]->x;
	py[i] = (GLfloat) inControl[i - 1]->y;
    }
    px[inOrder] = (GLfloat) inVecTo->x;
    py[inOrder] = (GLfloat) inVecTo->y;

    firstNode->x = px[0];
    firstNode->y = py[0];
    firstNode->t = 0.;
    firstNode->next = currentNode;
    
    currentNode->x = px[inOrder];
    currentNode->y = py[inOrder];
    currentNode->t = 1.;
    currentNode->next = NULL;
    
    while (loop) {
	GLfloat projx = 0., projy = 0.;
	__glcNode newNode;
	GLfloat s = 0., t = 0., distance = 0.;
	GLint j = 0;
	GLint n = 0, ind = 0;
	
	t = (firstNode->t + currentNode->t) / 2.;
	
	n = inOrder;
	
	for (i = 0; i < inOrder ; i++) {
	    for (j = 0; j < inOrder - i; j++) {
		ind = n - inOrder + i + j;
		px[n + j + 1] = (1 - t) * px[ind] + t * px[ind + 1];
		py[n + j + 1] = (1 - t) * py[ind] + t * py[ind + 1];
	    }
	    n += inOrder - i;
	}
	
	newNode.x = px[n];
	newNode.y = py[n];
	newNode.t = t;

	/* Compute the coordinate of the projection of 'new_node' onto the chord */
	s = ((newNode.x - firstNode->x) * (currentNode->x - firstNode->x)
	   + (newNode.y - firstNode->y) * (currentNode->y - firstNode->y))
	   / ((currentNode->x - firstNode->x) * (currentNode->x - firstNode->x)
	   +  (currentNode->y - firstNode->y) * (currentNode->y - firstNode->y));

	projx = firstNode->x + s * (currentNode->x - firstNode->x);
	projy = firstNode->y + s * (currentNode->y - firstNode->y);

	/* Compute the chordal distance */
	distance = (projx - newNode.x) * (projx - newNode.x)
		 + (projy - newNode.y) * (projy - newNode.y);
	
	/* If the chordal distance is greater than the tolerance then we
	   split the chord */
	if (distance > data->tolerance) {
	    /* Add the current node to the list (i.e. store the next
		chord ends) */
	    endList->x = newNode.x;
	    endList->y = newNode.y;
	    endList->t = newNode.t;

	    /* The current chord ends are now 'first_node' and 'new_node' */
	    firstNode->next = endList;
	    endList->next = currentNode;
	    currentNode = endList;

	    endList++;
	}
	else {
	    GLdouble *vertex = &data->vertex[data->numVertex][0];
    
	    vertex[0] = currentNode->x * data->scale;
	    vertex[1] = currentNode->y * data->scale;
	    vertex[2] = 0.;
	    gluTessVertex(data->tess, vertex, vertex);
	    data->numVertex++;
	
	    if (currentNode->next) {
		/* Prepare to examine the next chord */
		firstNode = currentNode;
		currentNode = currentNode->next;
	    }
	    else
		/* No next chord : exit */
		loop = GL_FALSE;
	}
    }
}

static int __glcMoveTo(FT_Vector *inVecTo, void* inUserData)
{
    __glcRendererData *data = (__glcRendererData *) inUserData;
    
    gluTessEndContour(data->tess);
    gluTessBeginContour(data->tess);
    
    data->pen = *inVecTo;
    return 0;
}

static int __glcLineTo(FT_Vector *inVecTo, void* inUserData)
{
    __glcRendererData *data = (__glcRendererData *) inUserData;
    GLdouble *vertex = &data->vertex[data->numVertex][0];
    
    vertex[0] = (GLdouble) inVecTo->x * data->scale;
    vertex[1] = (GLdouble) inVecTo->y * data->scale;
    vertex[2] = 0.;
    gluTessVertex(data->tess, vertex, vertex);
    data->numVertex++;
    
    data->pen = *inVecTo;
    return 0;
}

static int __glcConicTo(FT_Vector *inVecControl, FT_Vector *inVecTo, void* inUserData)
{
    __glcRendererData *data = (__glcRendererData *) inUserData;
    FT_Vector *control[1];
    
    control[0] = inVecControl;
    __glcdeCasteljau(inVecTo, control, inUserData, 2);

    data->pen = *inVecTo;
    return 0;
}

static int __glcCubicTo(FT_Vector *inVecControl1, FT_Vector *inVecControl2, FT_Vector *inVecTo, void* inUserData)
{
    __glcRendererData *data = (__glcRendererData *) inUserData;
    FT_Vector *control[2];
    
    control[0] = inVecControl1;
    control[1] = inVecControl2;
    __glcdeCasteljau(inVecTo, control, inUserData, 3);

    data->pen = *inVecTo;
    return 0;
}

static void __glcCallbackError(GLenum inErrorCode)
{
    __glcRaiseError(GLC_RESOURCE_ERROR);
}

void __glcRenderCharScalable(__glcFont* inFont, __glcContextState* inState,
			     GLint inCode, GLboolean inFill)
{
    FT_Outline *outline = NULL;
    FT_Outline_Funcs interface;
    FT_List list = NULL;
    FT_ListNode node = NULL;
    __glcDisplayListKey *dlKey = NULL;
    
    __glcRendererData rendererData;
    GLUtesselator *tess = gluNewTess();

    outline = &inFont->face->glyph->outline;
    interface.shift = 0;
    interface.delta = 0;
    interface.move_to = __glcMoveTo;
    interface.line_to = __glcLineTo;
    interface.conic_to = __glcConicTo;
    interface.cubic_to = __glcCubicTo;
    
    rendererData.tess = tess;
    rendererData.numVertex = 0;

    /* grid_coordinate is given in 26.6 fixed point integer hence we
       divide the scale by 2^6 */
    rendererData.scale = 1./64.;

    /* We convert the user given tolerance into sensible units */
    rendererData.tolerance = inState->resolution * 64. * inFont->face->units_per_EM;
    
    /* FIXME : may be we should use a bigger array ? */
    rendererData.vertex = (GLdouble (*)[3])__glcMalloc(GLC_MAX_VERTEX * sizeof(GLfloat) * 3);
    if (!rendererData.vertex) {
	gluDeleteTess(tess);
	__glcRaiseError(GLC_RESOURCE_ERROR);
	return;
    }
    
    gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
    gluTessProperty(tess, GLU_TESS_BOUNDARY_ONLY, !inFill);

    gluTessCallback(tess, GLU_TESS_ERROR, (void (*) ())__glcCallbackError);
    gluTessCallback(tess, GLU_TESS_BEGIN, (void (*) ())glBegin);
    gluTessCallback(tess, GLU_TESS_VERTEX, (void (*) ())glVertex3dv);
    gluTessCallback(tess, GLU_TESS_END, glEnd);
	
    gluTessNormal(tess, 0., 0., 1.);

    if (inState->glObjects) {
      dlKey = (__glcDisplayListKey *)__glcMalloc(sizeof(__glcDisplayListKey));
      if (!dlKey) {
	__glcRaiseError(GLC_RESOURCE_ERROR);
	return;
      }

      dlKey->list = glGenLists(1);
      dlKey->face = inFont->faceID;
      dlKey->code = inCode;
      dlKey->renderMode = inFill ? 4 : 3;

      /* Get (or create) a new entry that contains the display list and store
       * the key in it
       */
      list = inFont->parent->displayList;
      if (!list) {
	list = (FT_List)__glcMalloc(sizeof(FT_ListRec));
	if (!list) {
	  __glcRaiseError(GLC_RESOURCE_ERROR);
	  __glcFree(dlKey);
	  return;
	}
	list->head = NULL;
	list->tail = NULL;
	inFont->parent->displayList = list;
      }

      node = (FT_ListNode)__glcMalloc(sizeof(FT_ListNodeRec));
      if (!node) {
	__glcRaiseError(GLC_RESOURCE_ERROR);
	__glcFree(dlKey);
	return;
      }
      node->data = dlKey;
      FT_List_Add(list, node);

      glNewList(dlKey->list, GL_COMPILE);
    }

    gluTessBeginPolygon(tess, &rendererData);
    gluTessBeginContour(tess);
    FT_Outline_Decompose(outline, &interface, &rendererData);
    gluTessEndContour(tess);
    gluTessEndPolygon(tess);
    
    glTranslatef(inFont->face->glyph->advance.x * rendererData.scale, 0., 0.);
    
    if (inState->glObjects) {
      glEndList();
      inState->listObjectCount++;
      glCallList(dlKey->list);
    }

    gluDeleteTess(tess);
    
    __glcFree(rendererData.vertex);
}
