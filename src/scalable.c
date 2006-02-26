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

/* Draws characters of scalable fonts */

#if defined __APPLE__ && defined __MACH__
#include <OpenGL/glu.h>
#else
#include <GL/glu.h>
#endif
#include <math.h>

#include "internal.h"
#include FT_OUTLINE_H
#include FT_LIST_H

typedef struct {
  FT_Vector pen;			/* Current coordinates */
  GLdouble tolerance;			/* Chordal tolerance */
  __glcArray* vertexArray;		/* Array of vertices */
  __glcArray* controlPoints;		/* Array of control points */
  __glcArray* endContour;		/* Array of contour limits */
  GLdouble scale_x;			/* Scale to convert grid point coordinates.. */
  GLdouble scale_y;			/* ..into pixel coordinates */
  GLdouble transformMatrix[16];		/* Transformation matrix from the object space
					   to the viewport */
  GLdouble viewport[4];			/* Viewport transformation */
  GLboolean displayListIsBuilding;	/* Is a display list planned to be built ? */
}__glcRendererData;

static void __glcComputePixelCoordinates(GLdouble* inCoord,
					 __glcRendererData* inData)
{
  GLdouble x = 0., y = 0., w = 0.;
  GLdouble norm = 0.;

  x = inCoord[0] * inData->transformMatrix[0]
    + inCoord[1] * inData->transformMatrix[4]
    + inData->transformMatrix[12];
  y = inCoord[0] * inData->transformMatrix[1]
    + inCoord[1] * inData->transformMatrix[5]
    + inData->transformMatrix[13];
  w = inCoord[0] * inData->transformMatrix[3]
    + inCoord[1] * inData->transformMatrix[7]
    + inData->transformMatrix[15];

  /* If w is very small compared to x and y this probably mean that the
   * transformation matrix is ill-conditioned (i.e. its determinant is
   * numerically null)
   */
  norm = sqrt(x * x + y * y);
  if (w < norm * GLC_EPSILON)
    w = norm * GLC_EPSILON; /* Ugly hack to handle the singularity of w */

  inCoord[2] = x * inData->viewport[0];
  inCoord[3] = y * inData->viewport[2];
  inCoord[4] = w;
  inCoord[5] = inCoord[2] / w;
  inCoord[6] = inCoord[3] / w;
}

/* __glcdeCasteljau : 
 *	renders bezier curves using the de Casteljau subdivision algorithm
 *
 * This function creates a piecewise linear curve which is close enough
 * to the real Bezier curve. The piecewise linear curve is built so that
 * the chordal distance is lower than a tolerance value.
 * The chordal distance is taken to be the perpendicular distance from the
 * parametric midpoint, (t = 0.5), to the chord. This may not always be
 * correct, but, in the small lengths which are being considered, this is good
 * enough. In order to mitigate any error, the chordal tolerance is taken to be
 * slightly smaller than the tolerance specified by the user.
 * A second simplifying assumption is that when too large a tolerance is
 * encountered, the chord is split at the parametric midpoint, rather than
 * guessing the exact location of the best chord. This could lead to slightly
 * sub-optimal lines, but it provides a fast method for choosing the
 * subdivision point. This guess can be refined by lengthening the lines.
 */ 
static int __glcdeCasteljau(FT_Vector *inVecTo, FT_Vector **inControl,
			    void *inUserData, GLint inOrder)
{
  __glcRendererData *data = (__glcRendererData *) inUserData;
  GLdouble(*controlPoint)[7] = NULL;
  GLdouble cp[7] = {0., 0., 0., 0., 0., 0., 0.};
  GLint i = 0, nArc = 1, arc = 0, rank = 0;
  GLdouble xMin = 0., xMax = 0., yMin =0., yMax = 0.;

  /* Append the control points to the vertex array */
  cp[0] = (GLdouble) data->pen.x * data->scale_x;
  cp[1] = (GLdouble) data->pen.y * data->scale_y;
  __glcComputePixelCoordinates(cp, data);
  if (!__glcArrayAppend(data->controlPoints, cp)) {
    __glcRaiseError(GLC_RESOURCE_ERROR);
    GLC_ARRAY_LENGTH(data->controlPoints) = 0;
    return 1;
  }

  xMin = cp[5];
  xMax = cp[5];
  yMin = cp[6];
  yMax = cp[6];

  /* Append the first vertex of the curve to the vertex array */
  rank = GLC_ARRAY_LENGTH(data->vertexArray);
  cp[2] = 0.;
  if (!__glcArrayAppend(data->vertexArray, cp)) {
    __glcRaiseError(GLC_RESOURCE_ERROR);
    GLC_ARRAY_LENGTH(data->controlPoints) = 0;
    return 1;
  }

  for (i = 1; i < inOrder; i++) {
    cp[0] = (GLdouble) inControl[i-1]->x * data->scale_x;
    cp[1] = (GLdouble) inControl[i-1]->y * data->scale_y;
    __glcComputePixelCoordinates(cp, data);
    if (!__glcArrayAppend(data->controlPoints, cp)) {
      __glcRaiseError(GLC_RESOURCE_ERROR);
      GLC_ARRAY_LENGTH(data->controlPoints) = 0;
      GLC_ARRAY_LENGTH(data->vertexArray) = rank;
      return 1;
    }
    xMin = (cp[5] < xMin ? cp[5] : xMin);
    xMax = (cp[5] > xMax ? cp[5] : xMax);
    yMin = (cp[6] < yMin ? cp[6] : yMin);
    yMax = (cp[6] > yMax ? cp[6] : yMax);
  }

  cp[0] = (GLdouble) inVecTo->x * data->scale_x;
  cp[1] = (GLdouble) inVecTo->y * data->scale_y;
  __glcComputePixelCoordinates(cp, data);
  if (!__glcArrayAppend(data->controlPoints, cp)) {
    __glcRaiseError(GLC_RESOURCE_ERROR);
    GLC_ARRAY_LENGTH(data->controlPoints) = 0;
    GLC_ARRAY_LENGTH(data->vertexArray) = rank;
    return 1;
  }
  xMin = (cp[5] < xMin ? cp[5] : xMin);
  xMax = (cp[5] > xMax ? cp[5] : xMax);
  yMin = (cp[6] < yMin ? cp[6] : yMin);
  yMax = (cp[6] > yMax ? cp[6] : yMax);

  /* controlPoint[] must be computed there because data->controlPoints->data
   * may have been modified by a realloc() in __glcArrayInsert()
   */
  controlPoint = (GLdouble(*)[7])GLC_ARRAY_DATA(data->controlPoints);

  if (!data->displayListIsBuilding) {
    /* If the bounding box of the bezier curve lies outside the viewport then
     * the bezier curve is replaced by a piecewise linear curve that joins the
     * control points
     */
    if ((xMin > data->viewport[0]) || (xMax < -data->viewport[0])
        || (yMin > data->viewport[2]) || (yMax < -data->viewport[2])) {
      for (i = 1; i < inOrder; i++) {
	controlPoint[i][2] = 0.;
	if (!__glcArrayAppend(data->vertexArray, controlPoint[i])) {
	  __glcRaiseError(GLC_RESOURCE_ERROR);
	  GLC_ARRAY_LENGTH(data->controlPoints) = 0;
	  GLC_ARRAY_LENGTH(data->vertexArray) = rank;
	  return 1;
	}
      }
      GLC_ARRAY_LENGTH(data->controlPoints) = 0;
      return 0;
    }
  }

  while(arc != nArc) {
    GLint j = 0;
    GLdouble dmax = 0.;
    GLdouble ax = controlPoint[0][5];
    GLdouble ay = controlPoint[0][6];
    GLdouble abx = controlPoint[inOrder][5] - ax;
    GLdouble aby = controlPoint[inOrder][6] - ay;

    /* Normalize AB */
    GLdouble normab = sqrt(abx * abx + aby * aby);
    abx /= normab;
    aby /= normab;

    /* For each control point, compute its chordal distance that is its
     * distance from the line between the first and the last control points
     */
    for (i = 1; i < inOrder; i++) {
      GLdouble cpx = controlPoint[i][5] - ax;
      GLdouble cpy = controlPoint[i][6] - ay;
      GLdouble s = cpx * abx + cpy * aby;
      GLdouble projx = s * abx - cpx;
      GLdouble projy = s * aby - cpy;

      /* Compute the chordal distance */
      GLdouble distance = projx * projx + projy * projy;

      dmax = distance > dmax ? distance : dmax;
    }

    if (dmax < data->tolerance) {
      arc++; /* Process the next arc */
      controlPoint = ((GLdouble(*)[7])GLC_ARRAY_DATA(data->controlPoints))
	+ arc * inOrder;
      /* Update the place where new vertices will be inserted in the vertex
       * array
       */
      rank++;
    }
    else {
      /* Split an arc into two smaller arcs (this is the actual de Casteljau
       * algorithm)
       */
      for (i = 0; i < inOrder; i++) {
	GLdouble *p1 = controlPoint[i];
	GLdouble *p2 = controlPoint[i+1];

	cp[0] = 0.5*(p1[0]+p2[0]);
	cp[1] = 0.5*(p1[1]+p2[1]);
	cp[2] = 0.5*(p1[2]+p2[2]);
	cp[3] = 0.5*(p1[3]+p2[3]);
	cp[4] = 0.5*(p1[4]+p2[4]);
	cp[5] = cp[2] / cp[4];
	cp[6] = cp[3] / cp[4];
	if (!__glcArrayInsert(data->controlPoints, arc*inOrder+i+1, cp)) {
	  __glcRaiseError(GLC_RESOURCE_ERROR);
	  GLC_ARRAY_LENGTH(data->controlPoints) = 0;
	  return 1;
	}

	/* controlPoint[] must be updated there because
	 * data->controlPoints->data may have been modified by a realloc() in
	 * __glcArrayInsert()
	 */
	controlPoint = ((GLdouble(*)[7])GLC_ARRAY_DATA(data->controlPoints))
	  + arc * inOrder;

	for (j = 0; j < inOrder - i - 1; j++) {
	  p1 = controlPoint[i+j+2];
	  p2 = controlPoint[i+j+3];
	  p1[0] = 0.5*(p1[0]+p2[0]);
	  p1[1] = 0.5*(p1[1]+p2[1]);
	  p1[2] = 0.5*(p1[2]+p2[2]);
	  p1[3] = 0.5*(p1[3]+p2[3]);
	  p1[4] = 0.5*(p1[4]+p2[4]);
	  p1[5] = p1[2] / p1[4];
	  p1[6] = p1[3] / p1[4];
	}
      }

      /* The point in cp[] is a point located on the Bezier curve : it must be
       * added to the vertex array
       */
      cp[2] = 0.;
      if (!__glcArrayInsert(data->vertexArray, rank+1, cp)) {
	__glcRaiseError(GLC_RESOURCE_ERROR);
	GLC_ARRAY_LENGTH(data->controlPoints) = 0;
	return 1;
      }

      nArc++; /* A new arc has been defined */
    }
  }

  /* The array of control points must be emptied in order to be ready for the
   * next call to the de Casteljau routine
   */
  GLC_ARRAY_LENGTH(data->controlPoints) = 0;

  return 0;
}

static int __glcMoveTo(FT_Vector *inVecTo, void* inUserData)
{
  __glcRendererData *data = (__glcRendererData *) inUserData;

  /* We don't need to store the point where the pen is since glyphs are defined
   * by closed loops (i.e. the first point and the last point are the same) and
   * the first point will be stored by the next call to lineto/conicto/cubicto.
   */

  if (!__glcArrayAppend(data->endContour,
			&GLC_ARRAY_LENGTH(data->vertexArray))) {
    __glcRaiseError(GLC_RESOURCE_ERROR);
    return 1;
  }

  data->pen = *inVecTo;
  return 0;
}

static int __glcLineTo(FT_Vector *inVecTo, void* inUserData)
{
  __glcRendererData *data = (__glcRendererData *) inUserData;
  GLdouble vertex[3];

  vertex[0] = (GLdouble) data->pen.x * data->scale_x;
  vertex[1] = (GLdouble) data->pen.y * data->scale_y;
  vertex[2] = 0.;
  if (!__glcArrayAppend(data->vertexArray, vertex)) {
    __glcRaiseError(GLC_RESOURCE_ERROR);
    return 1;
  }

  data->pen = *inVecTo;
  return 0;
}

static int __glcConicTo(FT_Vector *inVecControl, FT_Vector *inVecTo,
			void* inUserData)
{
  __glcRendererData *data = (__glcRendererData *) inUserData;
  FT_Vector *control[1];
  int error = 0;

  control[0] = inVecControl;
  error = __glcdeCasteljau(inVecTo, control, inUserData, 2);
  data->pen = *inVecTo;

  return error;
}

static int __glcCubicTo(FT_Vector *inVecControl1, FT_Vector *inVecControl2,
			FT_Vector *inVecTo, void* inUserData)
{
  __glcRendererData *data = (__glcRendererData *) inUserData;
  FT_Vector *control[2];
  int error = 0;

  control[0] = inVecControl1;
  control[1] = inVecControl2;
  error = __glcdeCasteljau(inVecTo, control, inUserData, 3);
  data->pen = *inVecTo;

  return error;
}

/*static void __glcCombineCallback(GLdouble coords[3], void* vertex_data[4],
				 GLfloat weight[4], void** outData,
				 void* inUserData)
{
  __glcRendererData *data = (__glcRendererData*)inUserData;
  GLdouble(*vertexArray)[3] = (GLdouble(*)[3])data->vertexArray->data;

  if (!__glcArrayAppend(data->vertexArray, coords)) {
    __glcRaiseError(GLC_RESOURCE_ERROR);
    return;
  }

  *outData = vertexArray[data->vertexArray->length-1];
}*/

static void __glcCallbackError(GLenum inErrorCode)
{
  __glcRaiseError(GLC_RESOURCE_ERROR);
}

void __glcRenderCharScalable(__glcFont* inFont, __glcContextState* inState,
			     GLint inCode, GLCenum inRenderMode, FT_Face inFace)
{
  FT_Outline *outline = NULL;
  FT_Outline_Funcs interface;
  FT_ListNode node = NULL;
  __glcDisplayListKey *dlKey = NULL;
  __glcRendererData rendererData;
  GLdouble projectionMatrix[16] = {1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.,
				   0., 0., 0., 0., 1.};
  GLdouble modelviewMatrix[16] = {1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1.,
				  0., 0., 0., 0., 1.};
  GLint listIndex = 0;
  int i = 0, j = 0;

  outline = &inFace->glyph->outline;
  interface.shift = 0;
  interface.delta = 0;
  interface.move_to = __glcMoveTo;
  interface.line_to = __glcLineTo;
  interface.conic_to = __glcConicTo;
  interface.cubic_to = __glcCubicTo;

  /* grid_coordinate is given in 26.6 fixed point integer hence we
     divide the scale by 2^6 */
  rendererData.scale_x = 1./64./GLC_POINT_SIZE;
  rendererData.scale_y = 1./64./GLC_POINT_SIZE;

  rendererData.vertexArray = inState->vertexArray;
  rendererData.controlPoints = inState->controlPoints;
  rendererData.endContour = inState->endContour;

  /* If no display list is planned to be built then compute distances in pixels
   * otherwise use the object space.
   */
  glGetIntegerv(GL_LIST_INDEX, &listIndex);

  if ((!inState->glObjects) && (!listIndex)) {
    GLint viewport[4];

    glGetDoublev(GL_MODELVIEW_MATRIX, modelviewMatrix);
    glGetDoublev(GL_PROJECTION_MATRIX, projectionMatrix);
    glGetIntegerv(GL_VIEWPORT, viewport);
    rendererData.viewport[0] = viewport[2] * 0.5;
    rendererData.viewport[1] = viewport[0] + viewport[2] * 0.5;
    rendererData.viewport[2] = viewport[3] * 0.5;
    rendererData.viewport[3] = viewport[1] + viewport[3] * 0.5;
    rendererData.tolerance = .25; /* Half pixel tolerance */
    rendererData.displayListIsBuilding = GL_FALSE;
  }
  else {
    /* Distances are computed in object space, so is the tolerance of the
     * de Casteljau algorithm.
     */
    rendererData.tolerance = 0.005 * GLC_POINT_SIZE * inFace->units_per_EM
      * rendererData.scale_x * rendererData.scale_y;
    rendererData.viewport[0] = 1.;
    rendererData.viewport[1] = 0.;
    rendererData.viewport[2] = 1.;
    rendererData.viewport[3] = 0.;
    rendererData.displayListIsBuilding = GL_TRUE;
  }


  /* Compute the matrix that transforms object space coordinates to viewport
   * coordinates. If we plan to use object space coordinates, this matrix is
   * set to identity.
   */
  for (i = 0; i < 4 ; i++) {
    for (j = 0; j < 4; j++) {
      rendererData.transformMatrix[i*4+j] =
	modelviewMatrix[i*4+0]*projectionMatrix[0*4+j]+
	modelviewMatrix[i*4+1]*projectionMatrix[1*4+j]+
	modelviewMatrix[i*4+2]*projectionMatrix[2*4+j]+
	modelviewMatrix[i*4+3]*projectionMatrix[3*4+j];
    }
  }

  /* Parse the outline of the glyph */
  if (FT_Outline_Decompose(outline, &interface, &rendererData)) {
    __glcRaiseError(GLC_RESOURCE_ERROR);
    GLC_ARRAY_LENGTH(inState->vertexArray) = 0;
    GLC_ARRAY_LENGTH(inState->endContour) = 0;
    return;
  }

  if (!__glcArrayAppend(rendererData.endContour,
			&GLC_ARRAY_LENGTH(rendererData.vertexArray))) {
    __glcRaiseError(GLC_RESOURCE_ERROR);
    GLC_ARRAY_LENGTH(inState->vertexArray) = 0;
    GLC_ARRAY_LENGTH(inState->endContour) = 0;
    return;
  }

  if (inState->glObjects) {
    dlKey = (__glcDisplayListKey *)__glcMalloc(sizeof(__glcDisplayListKey));
    if (!dlKey) {
      __glcRaiseError(GLC_RESOURCE_ERROR);
      GLC_ARRAY_LENGTH(inState->vertexArray) = 0;
      GLC_ARRAY_LENGTH(inState->endContour) = 0;
      return;
    }

    dlKey->list = glGenLists(1);
    dlKey->faceDesc = inFont->faceDesc;
    dlKey->code = inCode;
    dlKey->renderMode = inRenderMode;

    /* Get (or create) a new entry that contains the display list and store
     * the key in it
     */
    node = (FT_ListNode)dlKey;
    node->data = dlKey;
    FT_List_Add(&inFont->parent->displayList, node);

    glNewList(dlKey->list, GL_COMPILE);
  }

  if (inRenderMode == GLC_TRIANGLE) {
    GLUtesselator *tess = gluNewTess();
    int i = 0, j = 0;
    int* endContour = (int*)GLC_ARRAY_DATA(rendererData.endContour);
    GLdouble (*vertexArray)[3] =
      (GLdouble(*)[3])GLC_ARRAY_DATA(rendererData.vertexArray);

    gluTessProperty(tess, GLU_TESS_WINDING_RULE, GLU_TESS_WINDING_ODD);
    gluTessProperty(tess, GLU_TESS_BOUNDARY_ONLY, GL_FALSE);

    gluTessCallback(tess, GLU_TESS_ERROR, (void (*) ())__glcCallbackError);
    gluTessCallback(tess, GLU_TESS_BEGIN, (void (*) ())glBegin);
    gluTessCallback(tess, GLU_TESS_VERTEX, (void (*) ())glVertex3dv);
/*    gluTessCallback(tess, GLU_TESS_COMBINE_DATA,
		    (void (*) ())__glcCombineCallback);*/
    gluTessCallback(tess, GLU_TESS_END, glEnd);

    gluTessNormal(tess, 0., 0., 1.);

    gluTessBeginPolygon(tess, &rendererData);

    for (i = 0; i < GLC_ARRAY_LENGTH(rendererData.endContour)-1; i++) {
      gluTessBeginContour(tess);
      for (j = endContour[i]; j < endContour[i+1]; j++)
	gluTessVertex(tess, vertexArray[j], vertexArray[j]);
      gluTessEndContour(tess);
    }

    gluTessEndPolygon(tess);

    gluDeleteTess(tess);
  }
  else {
    int i = 0;
    int* endContour = (int*)GLC_ARRAY_DATA(rendererData.endContour);

    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);
    glNormal3f(0., 0., 1.);
    glVertexPointer(3, GL_DOUBLE, 0, GLC_ARRAY_DATA(rendererData.vertexArray));

    for (i = 0; i < GLC_ARRAY_LENGTH(rendererData.endContour)-1; i++)
      glDrawArrays(GL_LINE_LOOP, endContour[i], endContour[i+1]-endContour[i]);

    glPopClientAttrib();
  }

  glTranslatef(inFace->glyph->advance.x * rendererData.scale_x,
	       inFace->glyph->advance.y * rendererData.scale_y, 0.);

  if (inState->glObjects) {
    glEndList();
    glCallList(dlKey->list);
  }

  GLC_ARRAY_LENGTH(inState->vertexArray) = 0;
  GLC_ARRAY_LENGTH(inState->endContour) = 0;
}
