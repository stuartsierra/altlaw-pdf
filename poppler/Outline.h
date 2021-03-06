//========================================================================
//
// Outline.h
//
// Copyright 2002-2003 Glyph & Cog, LLC
//
//========================================================================

#ifndef OUTLINE_H
#define OUTLINE_H

#ifdef USE_GCC_PRAGMAS
#pragma interface
#endif

#include "Object.h"
#include "CharTypes.h"

class GooString;
class GooList;
class XRef;
class LinkAction;

//------------------------------------------------------------------------

class Outline {
public:

  Outline(Object *outlineObj, XRef *xref);
  ~Outline();

  GooList *getItems() { return items; }

private:

  GooList *items;		// NULL if document has no outline,
				// otherwise, a list of OutlineItem
};

//------------------------------------------------------------------------

class OutlineItem {
public:

  OutlineItem(Dict *dict, XRef *xrefA);
  ~OutlineItem();

  static GooList *readItemList(Object *firstItemRef, Object *lastItemRef,
			     XRef *xrefA);

  void open();
  void close();

  Unicode *getTitle() { return title; }
  int getTitleLength() { return titleLen; }
  LinkAction *getAction() { return action; }
  GBool isOpen() { return startsOpen; }
  GBool hasKids() { return firstRef.isRef(); }
  GooList *getKids() { return kids; }

private:

  XRef *xref;
  Unicode *title;
  int titleLen;
  LinkAction *action;
  Object firstRef;
  Object lastRef;
  Object nextRef;
  GBool startsOpen;
  GooList *kids;	// NULL if this item is closed or has no kids,
			// otherwise a list of OutlineItem
};

#endif
