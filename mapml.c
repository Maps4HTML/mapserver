/******************************************************************************
 * $Id$
 *
 * Project:  MapServer
 * Purpose:  MapML support implementation. 
 *           See https://maps4html.github.io/HTML-Map-Element/spec/
 * Author:   Daniel Morissette
 *
 ******************************************************************************
 * Copyright (c) 1996-2019 Regents of the University of Minnesota.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies of this Software or works derived from this Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

#define _GNU_SOURCE

#include "mapserver.h"
#include "mapows.h"

#include "cpl_string.h"  /* CPLSPrintf() */

/* There is a dependency on libxml2 for XML handling */
#ifdef USE_LIBXML2
#include "maplibxml2.h"
#endif

/* 
 * Utility function to output MapML doc via msIO
 */

static int _msIO_MapMLDump(FILE *fp, xmlDocPtr psDoc)
{
  int status = MS_SUCCESS;

  /* Note: we want to avoid the <?xml declaration in the output, 
   * so we dump the root node instead of the document itself */
  xmlBufferPtr buf = xmlBufferCreate();
  if (xmlNodeDump(buf, psDoc, xmlDocGetRootElement(psDoc), 0, 1) > 0) {
    msIO_printf("%s\n", buf->content);
  }
  else {
    // TODO: Produce an exception
    msSetError(MS_WMSERR, "Writing MapML XML output failed.", "_msIO_MapMLDump()");
    status = MS_FAILURE;
  }
  xmlBufferFree (buf);

  return status;
}

/* A couple of useful XML-generation shortcuts */

static xmlNodePtr _xmlNewChild1Prop(xmlNodePtr parent_node, const char *node_name, const char *node_value, const char *prop1, const char *value1)
{
  xmlNodePtr _node = xmlNewChild(parent_node, NULL, BAD_CAST node_name, BAD_CAST node_value);
  if (value1) xmlNewProp(_node, BAD_CAST prop1, BAD_CAST value1);
  return _node;
}

static xmlNodePtr _xmlNewChild2Prop(xmlNodePtr parent_node, const char *node_name, const char *node_value, const char *prop1, const char *value1, const char *prop2, const char *value2)
{
  xmlNodePtr _node = xmlNewChild(parent_node, NULL, BAD_CAST node_name, BAD_CAST node_value);
  if (value1) xmlNewProp(_node, BAD_CAST prop1, BAD_CAST value1);
  if (value2) xmlNewProp(_node, BAD_CAST prop2, BAD_CAST value2);
  return _node;
}

static xmlNodePtr _xmlNewChild3Prop(xmlNodePtr parent_node, const char *node_name, const char *node_value, const char *prop1, const char *value1, const char *prop2, const char *value2, const char *prop3, const char *value3)
{
  xmlNodePtr _node = xmlNewChild(parent_node, NULL, BAD_CAST node_name, BAD_CAST node_value);
  if (value1) xmlNewProp(_node, BAD_CAST prop1, BAD_CAST value1);
  if (value2) xmlNewProp(_node, BAD_CAST prop2, BAD_CAST value2);
  if (value3) xmlNewProp(_node, BAD_CAST prop3, BAD_CAST value3);
  return _node;
}

static xmlNodePtr _xmlNewChild4Prop(xmlNodePtr parent_node, const char *node_name, const char *node_value, const char *prop1, const char *value1, const char *prop2, const char *value2, const char *prop3, const char *value3, const char *prop4, const char *value4)
{
  xmlNodePtr _node = xmlNewChild(parent_node, NULL, BAD_CAST node_name, BAD_CAST node_value);
  if (value1) xmlNewProp(_node, BAD_CAST prop1, BAD_CAST value1);
  if (value2) xmlNewProp(_node, BAD_CAST prop2, BAD_CAST value2);
  if (value3) xmlNewProp(_node, BAD_CAST prop3, BAD_CAST value3);
  if (value4) xmlNewProp(_node, BAD_CAST prop4, BAD_CAST value4);
  return _node;
}

static xmlNodePtr _xmlNewChild5Prop(xmlNodePtr parent_node, const char *node_name, const char *node_value, const char *prop1, const char *value1, const char *prop2, const char *value2, const char *prop3, const char *value3, const char *prop4, const char *value4, const char *prop5, const char *value5)
{
  xmlNodePtr _node = xmlNewChild(parent_node, NULL, BAD_CAST node_name, BAD_CAST node_value);
  if (value1) xmlNewProp(_node, BAD_CAST prop1, BAD_CAST value1);
  if (value2) xmlNewProp(_node, BAD_CAST prop2, BAD_CAST value2);
  if (value3) xmlNewProp(_node, BAD_CAST prop3, BAD_CAST value3);
  if (value4) xmlNewProp(_node, BAD_CAST prop4, BAD_CAST value4);
  if (value5) xmlNewProp(_node, BAD_CAST prop5, BAD_CAST value5);
  return _node;
}

static void _xmlNewPropInt(xmlNodePtr _node, const char *prop1, int value1)
{
  xmlNewProp(_node, BAD_CAST prop1, BAD_CAST CPLSPrintf("%d", value1));
}

static void _xmlNewPropDouble(xmlNodePtr _node, const char *prop1, double value1)
{
  xmlNewProp(_node, BAD_CAST prop1, BAD_CAST CPLSPrintf("%g", value1));
}


/*
 * Map MapML Projection name to EPSG CRS codes.
 *
 * Returns corresponding EPSG/CRS code or
 * Returns NULL if projection is invalid or not enabled.
 *
 * Set bQuietMode = TRUE to silently return NULL if SRS not enabled.
 * Set bQuietMode = FALSE to produce an error if SRS is not enabled
 */
static const char * msIsMapMLProjectionEnabled(mapObj *map, layerObj *lp, const char *pszNamespaces, const char *pszProjection, int bQuietMode)
{
  const char *pszCRS=NULL;
  
  /* Validate PROJECTION and map it to WMS CRS */
  if (pszProjection && strcasecmp(pszProjection, "OSMTILE") == 0)
    pszCRS = "EPSG:3857";  // Web Mercator
  else if (pszProjection && strcasecmp(pszProjection, "CBMTILE") == 0)
    pszCRS = "EPSG:3978";  // Canada LCC
  else if (pszProjection && strcasecmp(pszProjection, "APSTILE") == 0)
    pszCRS = "EPSG:5936";  // Alaska Polar Stereographic
  else if (pszProjection && strcasecmp(pszProjection, "WGS84-4326") == 0) {
    pszCRS = "EPSG:4326";
  }
  else if (pszProjection && strcasecmp(pszProjection, "WGS84") == 0) {
    pszCRS = "CRS:84";
  }
  else {
    msSetError(MS_WMSERR, "Invalid PROJECTION parameter", "msMapMLProjection2EPSG()");
    return NULL;
  }

  if (!msOWSIsCRSValid2(map, lp, pszNamespaces, pszCRS)) {
    if (!bQuietMode)
      msSetError(MS_WMSERR, "PROJECTION %s requires CRS %s to be enabled for this layer.", "msMapMLProjection2EPSG()", pszProjection, pszCRS);
    return NULL;
  }

  return pszCRS;
}


/*
** msMapMLException()
**
** Report current MapServer error in requested format.
*/

int msMapMLException(mapObj *map, const char *exception_code)
{
  msIO_setHeader("Content-Type","text/xml; charset=UTF-8");
  msIO_sendHeaders();

  msIO_printf("<?xml version='1.0' encoding=\"UTF-8\" standalone=\"no\" ?>\n");
  msIO_printf("<ServiceExceptionReport>\n");

  if (exception_code)
    msIO_printf("<ServiceException code=\"%s\">\n", exception_code);
  else
    msIO_printf("<ServiceException>\n");
  msWriteErrorXML(stdout);
  msIO_printf("</ServiceException>\n");
  msIO_printf("</ServiceExceptionReport>\n");

  return MS_FAILURE; /* so that we can call 'return msMapMLException();' anywhere */
}


/*
** msWriteMapMLLayer()
**
** Return specified layer info in response to GetMapML requests.
**
** URL Parameters:
**  MAP=...
**  SERVICE=WMS (only WMS supported for now)
**  REQUEST=GetMapML
**  LAYER=...
**  STYLE=...
**  PROJECTION= one of OSMTILE, CBMTILE, APSTILE, WGS84
**              defaults to OSMTILE if not specified (as per spec)
**  MAPML_MODE= image (default) or tile or cgitile or features
**
**
** Returns MS_SUCCESS/MS_FAILURE
*/
int msWriteMapMLLayer(FILE *fp, mapObj *map, cgiRequestObj *req, owsRequestObj *ows_request, const char *pszService)
{
#ifdef USE_MAPML
  int i = 0;
  const char *pszLayer = NULL, *pszStyle = "";
  layerObj *lp;
  int nLayers =0;
  int iLayerIndex = -1;
  char ***nestedGroups = NULL;
  int *numNestedGroups = NULL;
  int *isUsedInNestedGroup = NULL;

  const char *pszProjection = "OSMTILE", *pszCRS=NULL;
  char *script_url = NULL;
  const char *pszVal1=NULL, *pszVal2=NULL;

  const char *pszEncoding = "UTF-8";
  const char *pszNamespaces = "MO";
  const char *pszMapMLMode = NULL;
  
  projectionObj proj;
  rectObj ext;
  
  /* We need this server's onlineresource. It will come with the trailing "?" or "&" */
  /* the returned string should be freed once we're done with it. */
  if ((script_url=msOWSGetOnlineResource(map, pszNamespaces, "onlineresource", req)) == NULL)  {
    msSetError(MS_WMSERR, "Missing OnlineResource.", "msWriteMapMLLayer()");
    return MS_FAILURE;
  }

  /* Process URL Parameters */
  for(i=0; map && req && i<req->NumParams; i++) {
    
    if(strcasecmp(req->ParamNames[i], "LAYER") == 0) {
      pszLayer = req->ParamValues[i];
    }
    
    if(strcasecmp(req->ParamNames[i], "PROJECTION") == 0) {
      pszProjection = req->ParamValues[i];
    }
    
    if(strcasecmp(req->ParamNames[i], "STYLE") == 0) {
      // TODO ideally we should validate that supplied style exists, see mapwms.c
      pszStyle = req->ParamValues[i];
    }
    
    if(strcasecmp(req->ParamNames[i], "MAPML_MODE") == 0) {
      pszMapMLMode = req->ParamValues[i];
    }
  }

  if (!pszLayer) {
    msSetError(MS_WMSERR, "Mandatory LAYER parameter missing in GetMapML request.", "msWriteMapMLLayer()");
    return MS_FAILURE;
  }


  /* Look for requested layer. we check for layer's and group's name */
  /* as well as wms_layer_group names */
  nestedGroups = (char***)msSmallCalloc(map->numlayers, sizeof(char**));
  numNestedGroups = (int*)msSmallCalloc(map->numlayers, sizeof(int));
  isUsedInNestedGroup = (int*)msSmallCalloc(map->numlayers, sizeof(int));
  msWMSPrepareNestedGroups(map, OWS_1_3_0, nestedGroups, numNestedGroups, isUsedInNestedGroup);

  for (i=0; i<map->numlayers; i++) {
    lp = GET_LAYER(map, i);
    if (  ((map->name && strcasecmp(map->name, pszLayer) == 0) ||
           (lp->name && strcasecmp(lp->name, pszLayer) == 0) ||
           (lp->group && strcasecmp(lp->group, pszLayer) == 0) ||
           ((numNestedGroups[i] >0) && (msStringInArray(pszLayer, nestedGroups[i], numNestedGroups[i]))) ) &&
          (msIntegerInArray(lp->index, ows_request->enabled_layers, ows_request->numlayers)) ) {
      nLayers++;
      lp->status = MS_ON;
      iLayerIndex = i;
      break; /* We only care about the first match */
    } else
      lp->status = MS_OFF;
  }

  /* free the stuff used for nested layers */
  for (i = 0; i < map->numlayers; i++) {
    if (numNestedGroups[i] > 0) {
      msFreeCharArray(nestedGroups[i], numNestedGroups[i]);
    }
  }
  msFree(nestedGroups);
  msFree(numNestedGroups);
  msFree(isUsedInNestedGroup);

  if (nLayers != 1) {
    msSetError(MS_WMSERR, "Invalid layer given in the LAYER parameter. A layer might be disabled for \
this request. Check wms/ows_enable_request settings.", "msWriteMapMLLayer()");
    return MS_FAILURE;
  }
  
  /* Validate MapML PROJECTION and map it to WMS CRS (EPSG code) */
  if ((pszCRS = msIsMapMLProjectionEnabled(map, lp, pszNamespaces, pszProjection, FALSE)) == NULL)
    return MS_FAILURE; // msSetError already called

  /* Fetch and reproject layer extent to requested CRS */
  // TODO: For now just using map extent... need to look up layer/group extent if applicable
  memcpy(&ext, &(map->extent), sizeof(rectObj));
  msInitProjection(&proj);
  if (msLoadProjectionStringEPSG(&proj, pszCRS) != 0) {
    /* Failed to load projection, msSetError shoudlshould already have been called */
    return MS_FAILURE;
  }
  if (msProjectionsDiffer(&(map->projection), &proj) == MS_TRUE) {
    msProjectRect(&(map->projection), &proj, &ext);
  } 
  
  
  /* 
   * We're good to go. Create a new <mapml> document and start populating the <head> and <body>
   */
  xmlDocPtr psMapMLDoc = NULL;       /* libxml2 document pointer */
  xmlNodePtr root_node = NULL, psMapMLHead = NULL, psMapMLBody = NULL, psNode = NULL;

  psMapMLDoc = xmlNewDoc(BAD_CAST "1.0");
  root_node = xmlNewNode(NULL, BAD_CAST "mapml");
  xmlDocSetRootElement(psMapMLDoc, root_node);

  psMapMLHead = xmlNewChild(root_node, NULL, BAD_CAST "head", NULL);
  psMapMLBody = xmlNewChild(root_node, NULL, BAD_CAST "body", NULL);

  /* *** mapmp/head *** */
  
  /* <title>: If LAYER name is the top-level map then return map title, otherwise return the first matching layer's title */
  if (map->name && EQUAL(map->name, pszLayer))
    pszVal1 = msOWSLookupMetadata3( &(map->web.metadata), NULL, pszNamespaces, "title", map->name );
  else
    pszVal1 = msOWSLookupMetadata3( &(lp->metadata), NULL, pszNamespaces, "title", lp->name );
  
  if (pszVal1)
    psNode = xmlNewChild(psMapMLHead, NULL, BAD_CAST "title", BAD_CAST pszVal1);

  /* <meta> */
  _xmlNewChild1Prop(psMapMLHead, "meta", NULL, "charset", pszEncoding);

  _xmlNewChild2Prop(psMapMLHead, "meta", NULL, "http-equiv", "Content-Type",
                    "content", CPLSPrintf("text/mapml;projection=%s", pszProjection) );

  /* link rel=license - mapped to *_attribution_* metadata */
  pszVal1 = msOWSLookupMetadata2( &(lp->metadata), &(map->web.metadata), pszNamespaces, "attribution_onlineresource" );
  pszVal2 = msOWSLookupMetadata2( &(lp->metadata), &(map->web.metadata), pszNamespaces, "attribution_title" );
  if (pszVal1 || pszVal2)
    _xmlNewChild3Prop(psMapMLHead, "link", NULL, "rel", "license", "href", pszVal1, "title", pszVal2);

  /* link rel=legend  */
  pszVal1 = CPLSPrintf("%sSERVICE=WMS&REQUEST=GetLegendGraphic&VERSION=1.3.0&FORMAT=image/png&LAYER=%s&STYLE=%s&SLD_VERSION=1.1.0", script_url, pszLayer, pszStyle);
  _xmlNewChild2Prop(psMapMLHead, "link", NULL, "rel", "legend", "href", pszVal1);

  /* link rel=alternate projection */
  const char *papszAllProj[] = {"OSMTILE", "CBMTILE", "APSTILE", "WGS84", NULL};
  const char *pszProj = *papszAllProj;
  for (int i=0; (pszProj=papszAllProj[i])!= NULL; i++) {
    if (!EQUAL(pszProjection, pszProj) &&
        msIsMapMLProjectionEnabled(map, lp, pszNamespaces, pszProj, TRUE) != NULL) {
      pszVal1 = CPLSPrintf("%sSERVICE=%s&REQUEST=GetMapML&LAYER=%s&STYLE=%s&PROJECTION=%s", script_url, pszService, pszLayer, pszStyle, pszProj);
      _xmlNewChild3Prop(psMapMLHead, "link", NULL, "rel", "alternate", "projection", pszProj, "href", pszVal1);
    }
  }
  

  /* *** mapml/body *** */

  /* What type of output do we want, controlled by mapml_wms_mode metadata, or request MAPML_MODE param 
   *  - image: (the default) produces full page WMS GetMap images
   *  - tile: produces link-rel = tile with tiled WMS GetMap requests
   *  - cgitile: produces mode=tile mapserv CGI requests
   *  - features: produces link-ref=features pointing to WFS GetFeature 
   */
  if (pszMapMLMode == NULL)
    pszMapMLMode = msOWSLookupMetadata2( &(lp->metadata), &(map->web.metadata), NULL, "mapml_wms_mode" );
  if (pszMapMLMode == NULL)
    pszMapMLMode = "image"; // The default
  
  /* <extent> */
  xmlNodePtr psMapMLExtent = _xmlNewChild1Prop(psMapMLBody, "extent", NULL, "units", pszProjection);

  if (EQUAL(pszMapMLMode, "tile")) {
    // MAPML TILE mode: Serve requested layer as tiled WMS GetMap requests
    // TODO: Should we allow this mode with WGS84?
    
    psNode = _xmlNewChild5Prop(psMapMLExtent, "input", NULL, "name", "txmin", "type", "location", "units", "tilematrix", "position", "top-left", "axis", "easting");
    _xmlNewPropDouble(psNode, "min", ext.minx);
    _xmlNewPropDouble(psNode, "max", ext.maxx);

    psNode = _xmlNewChild5Prop(psMapMLExtent, "input", NULL, "name", "tymin", "type", "location", "units", "tilematrix", "position", "bottom-left", "axis", "northing");
    _xmlNewPropDouble(psNode, "min", ext.miny);
    _xmlNewPropDouble(psNode, "max", ext.maxy);

    psNode = _xmlNewChild5Prop(psMapMLExtent, "input", NULL, "name", "txmax", "type", "location", "units", "tilematrix", "position", "top-right", "axis", "easting");
    _xmlNewPropDouble(psNode, "min", ext.minx);
    _xmlNewPropDouble(psNode, "max", ext.maxx);

    psNode = _xmlNewChild5Prop(psMapMLExtent, "input", NULL, "name", "tymax", "type", "location", "units", "tilematrix", "position", "top-left", "axis", "northing");
    _xmlNewPropDouble(psNode, "min", ext.miny);
    _xmlNewPropDouble(psNode, "max", ext.maxy);

    /* WMS BBOX format, coordinate default to X,Y, except for EPSG:4326 where it is lat,lon */
    const char *pszBBOX = "{txmin},{tymin},{txmax},{tymax}";
    if (EQUAL(pszCRS, "EPSG:4326"))
      pszBBOX = "{tymin},{txmin},{tymax},{txmax}";

    /* GetMap URL */
    // TODO: Set proper output format and transparency... special metadata?
    pszVal1 = CPLSPrintf("%sSERVICE=WMS&REQUEST=GetMap&FORMAT=image/png&TRANSPARENT=TRUE&VERSION=1.3.0&LAYERS=%s&STYLES=%s&WIDTH=256&HEIGHT=256&CRS=%s&BBOX=%s&m4h=t", script_url, pszLayer, pszStyle, pszCRS, pszBBOX);
    psNode = _xmlNewChild2Prop(psMapMLExtent, "link", NULL, "rel", "tile", "tref", pszVal1);

    // TODO: Add WMS GetFeatureInfo (share code with "image" case)
    
  }
  else if (EQUAL(pszMapMLMode, "cgitile")) {
    // MAPML CGITILE mode: Serve requested layer as tiles using mapserv CGI mode=tile&tilemode=gmap
    // TODO: is this use case valid only for the OSMTILE projection???
    
    // Special case to map top-level map layer in WMS to special keyword "all" in mapserv CGI syntax
    if (map->name && EQUAL(map->name, pszLayer))
      pszLayer = "all";

    // TODO: map to real zoom/axis values here
    psNode = _xmlNewChild2Prop(psMapMLExtent, "input", NULL, "name", "z", "type", "zoom");
    _xmlNewPropInt(psNode, "value", 10);
    _xmlNewPropInt(psNode, "min", 4);
    _xmlNewPropInt(psNode, "max", 15);

    psNode = _xmlNewChild4Prop(psMapMLExtent, "input", NULL, "name", "y", "type", "location", "units", "tilematrix", "axis", "row");
    _xmlNewPropInt(psNode, "min", 0);
    _xmlNewPropInt(psNode, "max", 32768);

    psNode = _xmlNewChild4Prop(psMapMLExtent, "input", NULL, "name", "x", "type", "location", "units", "tilematrix", "axis", "column");
    _xmlNewPropInt(psNode, "min", 0);
    _xmlNewPropInt(psNode, "max", 32768);

    pszVal1 = CPLSPrintf("%smode=tile&tilemode=gmap&FORMAT=image/png&LAYERS=%s&tile={x}+{y}+{z}&m4h=t", script_url, pszLayer);
    psNode = _xmlNewChild2Prop(psMapMLExtent, "link", NULL, "rel", "tile", "tref", pszVal1);
    
  }
  else if (EQUAL(pszMapMLMode, "image")) {
    // Produce full screen WMS GetMap requests

    _xmlNewChild2Prop(psMapMLExtent, "input", NULL, "name", "w", "type", "width");
    _xmlNewChild2Prop(psMapMLExtent, "input", NULL, "name", "h", "type", "height");

    psNode = _xmlNewChild5Prop(psMapMLExtent, "input", NULL, "name", "xmin", "type", "location", "units", "pcrs", "position", "top-left", "axis", "easting");
    _xmlNewPropDouble(psNode, "min", ext.minx);
    _xmlNewPropDouble(psNode, "max", ext.maxx);

    psNode = _xmlNewChild5Prop(psMapMLExtent, "input", NULL, "name", "ymin", "type", "location", "units", "pcrs", "position", "bottom-left", "axis", "northing");
    _xmlNewPropDouble(psNode, "min", ext.miny);
    _xmlNewPropDouble(psNode, "max", ext.maxy);

    psNode = _xmlNewChild5Prop(psMapMLExtent, "input", NULL, "name", "xmax", "type", "location", "units", "pcrs", "position", "top-right", "axis", "easting");
    _xmlNewPropDouble(psNode, "min", ext.minx);
    _xmlNewPropDouble(psNode, "max", ext.maxx);

    psNode = _xmlNewChild5Prop(psMapMLExtent, "input", NULL, "name", "ymax", "type", "location", "units", "pcrs", "position", "top-left", "axis", "northing");
    _xmlNewPropDouble(psNode, "min", ext.miny);
    _xmlNewPropDouble(psNode, "max", ext.maxy);

    /* WMS BBOX format, coordinate default to X,Y, except for EPSG:4326 where it is lat,lon */
    const char *pszBBOX = "{xmin},{ymin},{xmax},{ymax}";
    if (EQUAL(pszCRS, "EPSG:4326"))
      pszBBOX = "{ymin},{xmin},{ymax},{xmax}";

    /* GetMap URL */
    // TODO: Set proper output format and transparency... special metadata?
    pszVal1 = CPLSPrintf("%sSERVICE=WMS&REQUEST=GetMap&FORMAT=image/png&TRANSPARENT=TRUE&VERSION=1.3.0&LAYERS=%s&STYLES=%s&WIDTH={w}&HEIGHT={h}&CRS=%s&BBOX=%s&m4h=t", script_url, pszLayer, pszStyle, pszCRS, pszBBOX);
    psNode = _xmlNewChild2Prop(psMapMLExtent, "link", NULL, "rel", "image", "tref", pszVal1);


    /* If layer is queryable then enable GetFeatureInfo */
    // TODO Check if layer is queryable (also need to check top-level map, groups, nested groups)
    // TODO: Check if wms_getfeatureinfo_formatlist includes text/mapml */
    // TODO: handle optional feature count
    psNode = _xmlNewChild4Prop(psMapMLExtent, "input", NULL, "name", "i", "type", "location", "axis", "i", "units", "map");
    _xmlNewPropInt(psNode, "min", 0);
    _xmlNewPropInt(psNode, "max", 0);

    psNode = _xmlNewChild4Prop(psMapMLExtent, "input", NULL, "name", "j", "type", "location", "axis", "j", "units", "map");
    _xmlNewPropInt(psNode, "min", 0);
    _xmlNewPropInt(psNode, "max", 0);

    pszVal1 = CPLSPrintf("%sSERVICE=WMS&REQUEST=GetFeatureInfo&INFO_FORMAT=text/mapml&FEATURE_COUNT=1&TRANSPARENT=TRUE&VERSION=1.3.0&LAYERS=%s&STYLES=%s&QUERY_LAYERS=%s&WIDTH={w}&HEIGHT={h}&CRS=%s&BBOX=%s&x={i}&y={j}&m4h=t", script_url, pszLayer, pszStyle, pszLayer, pszCRS, pszBBOX);
    psNode = _xmlNewChild2Prop(psMapMLExtent, "link", NULL, "rel", "query", "tref", pszVal1);

  }
  else if (EQUAL(pszMapMLMode, "features")) {
    // MAPML FEATURES mode: Serve link to WFS GetFeature requests
    
    // TODO: WFS GetFeature not available yet
    
  }
   else {
    msSetError(MS_WMSERR, "Requested MapML output mode not supported. Use one of image, tile, cgitile or features.", "msWriteMapMLLayer()");
    return msMapMLException(map, "InvalidRequest");
  }
  

  /* Generate output */
  msIO_setHeader("Content-Type","text/mapml");
  msIO_sendHeaders();

  _msIO_MapMLDump(fp, psMapMLDoc);
 
  /* Cleanup */
  xmlFreeDoc(psMapMLDoc);
  msFree(script_url);
  
  return MS_SUCCESS;
#else
  msSetError(MS_WMSERR, "MapML support is not available.", "msWriteMapMLLayer()");
  return(MS_FAILURE);
#endif /* USE_MAPML */
}



/*
** msWriteMapMLLayer()
**
** Dump MapML query results for WMS GetFeatureInfo
**
**
** Returns MS_SUCCESS/MS_FAILURE
*/

//TODO: This is just a temporary implementation derived from msGMLWriteQuery. It will be rewritten once the OGR/mapml driver is available

int msWriteMapMLQuery(mapObj *map, FILE *fp, const char *namespaces)
{
#if defined(USE_MAPML)
  int status;
  int i,j,k;
  layerObj *lp=NULL;
  shapeObj shape;
  char szPath[MS_MAXPATHLEN];
  char *value;
  char *pszMapSRS = NULL;

  gmlGroupListObj *groupList=NULL;
  gmlItemListObj *itemList=NULL;
  gmlConstantListObj *constantList=NULL;
  gmlGeometryListObj *geometryList=NULL;
  gmlItemObj *item=NULL;
  gmlConstantObj *constant=NULL;

  msInitShape(&shape);


  msIO_setHeader("Content-Type","text/mapml");
  msIO_sendHeaders();

  msIO_fprintf(fp, "<mapml>\n");
  msIO_fprintf(fp, "  <head>\n");
  msIO_fprintf(fp, "  <title>GetFeatureInfo Results</title>\n");
  //msIO_fprintf(fp, "  <base href=\"TODO-href\" />\n");
  msIO_fprintf(fp, "  <meta charset=\"utf-8\" />\n");
  // TODO: Add PROJECTION param to Content-Type value
  msIO_fprintf(fp, "  <meta http-equiv=\"Content-Type\" content=\"text/mapml\" />\n");

  msIO_fprintf(fp, "  </head>\n");

  msIO_fprintf(fp, "  <body>\n");
  msIO_fprintf(fp, "    <extent />\n");  // Mandatory extent element (empty)

  /* Look up map SRS. We need an EPSG code for GML, if not then we get null and we'll fall back on the layer's SRS */
  msOWSGetEPSGProj(&(map->projection), NULL, namespaces, MS_TRUE, &pszMapSRS);

  /* step through the layers looking for query results */
  for(i=0; i<map->numlayers; i++) {
    char *pszOutputSRS = NULL;
    int nSRSDimension = 2;
    const char* geomtype;
    char *layername=NULL, *layertitle=NULL;
    
    lp = (GET_LAYER(map, map->layerorder[i]));

    if(lp->resultcache && lp->resultcache->numresults > 0) { /* found results */

#ifdef USE_PROJ
      /* Determine output SRS, if map has none, then try using layer's native SRS */
      if ((pszOutputSRS = pszMapSRS) == NULL) {
        msOWSGetEPSGProj(&(lp->projection), NULL, namespaces, MS_TRUE, &pszOutputSRS);
        if (pszOutputSRS == NULL) {
          msSetError(MS_WMSERR, "No valid EPSG code in map or layer projection for GML output", "msGMLWriteQuery()");
          continue;  /* No EPSG code, cannot output this layer */
        }
      }
#endif

      /* start this collection (layer) */
      
      // TODO: Lookup layername metadata, and chack for NULL lp->name
      /* if no layer name provided fall back on the layer name + "_layer" */
      layername = (char*) msSmallMalloc(strlen(lp->name)+7);
      sprintf(layername, "%s", lp->name);
      //msOWSPrintValidateMetadata(fp, &(lp->metadata), namespaces, "layername", OWS_NOERR, "\t<%s>\n", layername);

      layertitle = (char *) msOWSLookupMetadata(&(lp->metadata), "OM", "title");
      if (layertitle) {
        //msOWSPrintEncodeMetadata(fp, &(lp->metadata), namespaces, "title", OWS_NOERR, "\t<gml:name>%s</gml:name>\n", layertitle);
      }

      geomtype = msOWSLookupMetadata(&(lp->metadata), "OFG", "geomtype");
      if( geomtype != NULL && (strstr(geomtype, "25d") != NULL || strstr(geomtype, "25D") != NULL) )
      {
          msIO_fprintf(fp, "<!-- WARNING: 25d requested for layer '%s' but MapML only supports 2D. -->\n", lp->name);
      }

      /* populate item and group metadata structures */
      itemList = msGMLGetItems(lp, namespaces);
      constantList = msGMLGetConstants(lp, namespaces);
      groupList = msGMLGetGroups(lp, namespaces);
      geometryList = msGMLGetGeometries(lp, namespaces, MS_FALSE);
      if (itemList == NULL || constantList == NULL || groupList == NULL || geometryList == NULL) {
        msSetError(MS_MISCERR, "Unable to populate item and group metadata structures", "msGMLWriteQuery()");
        return MS_FAILURE;
      }

      for(j=0; j<lp->resultcache->numresults; j++) {
        status = msLayerGetShape(lp, &shape, &(lp->resultcache->results[j]));
        if(status != MS_SUCCESS) {
           msGMLFreeGroups(groupList);
           msGMLFreeConstants(constantList);
           msGMLFreeItems(itemList);
           msGMLFreeGeometries(geometryList);
           return(status);
        }

#ifdef USE_PROJ
        /* project the shape into the map projection (if necessary), note that this projects the bounds as well */
        if(pszOutputSRS == pszMapSRS && msProjectionsDiffer(&(lp->projection), &(map->projection))) {
          status = msProjectShape(&lp->projection, &map->projection, &shape);
          if(status != MS_SUCCESS) {
            msIO_fprintf(fp, "<!-- Warning: Failed to reproject shape: %s -->\n",msGetErrorString(","));
            continue;
          }
        }
#endif

        /* start this feature */
        msIO_fprintf(fp, "      <feature id=\"%s.%ld\" class=\"%s\">\n", layername, shape.index, layername);

        // TODO: handle geomeotry output...
        //
        /* Write the feature geometry and bounding box unless 'none' was requested. */
        /* Default to bbox only if nothing specified and output full geometry only if explicitly requested */
        /*  
        if(!(geometryList && geometryList->numgeometries == 1 && strcasecmp(geometryList->geometries[0].name, "none") == 0)) {
          gmlWriteBounds(fp, OWS_GML2, &(shape.bounds), pszOutputSRS, "\t\t\t", "gml");
          if (geometryList && geometryList->numgeometries > 0 )
            gmlWriteGeometry(fp, geometryList, OWS_GML2, &(shape), pszOutputSRS, NULL, "\t\t\t", "", nSRSDimension);
        }
        */
        
        /* write properties */
        msIO_fprintf(fp, "        <properties>\n");
        msIO_fprintf(fp, "          <table>\n");
        msIO_fprintf(fp, "            <thead>\n");
        msIO_fprintf(fp, "              <tr>\n");
        msIO_fprintf(fp, "                <th role=\"columnheader\" scope=\"col\">Property Name</th>\n");
        msIO_fprintf(fp, "                <th role=\"columnheader\" scope=\"col\">Property Value</th>\n");
        msIO_fprintf(fp, "              </tr>\n");
        msIO_fprintf(fp, "            </thead>\n");
        
        for(k=0; k<itemList->numitems; k++) {
          item = &(itemList->items[k]);
          if(item->visible && msItemInGroups(item->name, groupList) == MS_FALSE) {
            msIO_fprintf(fp, "            <tbody>\n");
            msIO_fprintf(fp, "              <tr>\n");
            msIO_fprintf(fp, "                <th scope=\"row\">%s</th>\n", item->name);
            msIO_fprintf(fp, "                <td itemprop=\"%s\">%s</td>\n", item->name, shape.values[k]);
            msIO_fprintf(fp, "              </tr>\n");
            msIO_fprintf(fp, "            </tbody>\n");
          }
        }

        msIO_fprintf(fp, "          </table>\n");
        msIO_fprintf(fp, "        </properties>\n");


        /* end this feature */
        msIO_fprintf(fp, "      </feature>\n");

        msFreeShape(&shape); /* init too */
      }

      /* end this collection (layer) */
      msFree(layername);
      msFree(layertitle);

      msGMLFreeGroups(groupList);
      msGMLFreeConstants(constantList);
      msGMLFreeItems(itemList);
      msGMLFreeGeometries(geometryList);

      /* msLayerClose(lp); */
    }
    if(pszOutputSRS!=pszMapSRS) {
      msFree(pszOutputSRS);
    }
  } /* next layer */


  
  /* end this document */
  msIO_fprintf(fp, "  </body>\n");
  msIO_fprintf(fp, "</mapml>\n");


  return(MS_SUCCESS);

#else
  msSetError(MS_MISCERR, "MapML support not enabled", "msWriteMapMLQuery()");
  return MS_FAILURE;
#endif
}


/*
** msMapMLTileDispatch() is the entry point for MAPMLTILE requests.
**
** Note: MapServer does not really support MAPMLTILE.
** This is only so that we can accept vendor-specific SERVICE=MAPMLTILE&REQUEST=GetMapML
**
** - If this is a valid request then it is processed and MS_SUCCESS is returned
**   on success, or MS_FAILURE on failure.
** - If this does not appear to be a valid WMTS request then MS_DONE
**   is returned and MapServer is expected to process this as a regular
**   MapServer request.
*/
int msMapMLTileDispatch(mapObj *map, cgiRequestObj *req, owsRequestObj *ows_request)
{
#ifdef USE_MAPML
  int i;
  const char *request=NULL, *service=NULL;

  /*
  ** Process Params
  */
  for(i=0; i<req->NumParams; i++) {
    if (strcasecmp(req->ParamNames[i], "REQUEST") == 0)
      request = req->ParamValues[i];
    else if (strcasecmp(req->ParamNames[i], "SERVICE") == 0)
      service = req->ParamValues[i];
   }

  /* If SERVICE is specified then it MUST be "MAPMLTILE" */
  if (service != NULL && strcasecmp(service, "MAPMLTILE") != 0)
    return MS_DONE;  /* Not a MAPMLTILE request */

  /*
  ** Dispatch request... 
  */
  if (request && strcasecmp(request, "GetMapML") == 0 ) {
    /* Return a MapML document for specified LAYER and PROJECTION
     * This is a vendor-specific extension, not a standard request.
     */
    msOWSRequestLayersEnabled(map, "MO", request, ows_request);
    if ( msWriteMapMLLayer(stdout, map, req, ows_request, "MAPMLTILE") != MS_SUCCESS )
      return msMapMLException(map, "InvalidRequest");
    /* Request completed */
    return MS_SUCCESS;
  }
  
  /* Hummmm... incomplete or unsupported request */
  if (service != NULL && strcasecmp(service, "MAPMLTILE") == 0) {
    msSetError(MS_WMSERR, "Incomplete or unsupported MAPMLTILE request", "msMapMTileDispatch()");
    return msMapMLException(map, "InvalidRequest");
  } else
    return MS_DONE;  /* Not a MAPMLTILE request */
#else
  msSetError(MS_WMSERR, "MAPMLTILE service support is not available.", "msMapMLTileDispatch()");
  return(MS_FAILURE);
#endif
}
