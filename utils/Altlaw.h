//========================================================================
//
// Altlaw.h
//
//========================================================================

#ifndef ALTLAW_H
#define ALTLAW_H

#ifdef __GNUC__
#pragma interface
#endif

#include <stdio.h>
#include <string.h>
#include "goo/gtypes.h"
#include "goo/GooList.h"
#include "goo/GooString.h"
#include "GfxFont.h"
#include "OutputDev.h"


// This should really be a function of the chosen DPI
// Assume 100dpi
#define ALTFUDGEY 10.0
#define ALTFUDGEX 8.0

class GfxState;
class GooString;

class AltString: public GooString {
  public:
  AltString(GooString *s, int page,
  			double x, double y,
  			double dx, double dy,
			double fontSize,
			GBool bold, GBool italics);
  ~AltString() {};

  // acts like cmp(str, arg)
  int cmpPYX(int page, double y, double x,
  		double fudgeY = ALTFUDGEY,
		double fudgeX = ALTFUDGEX );
  GBool isNum();
  GBool similar(AltString *str);

  int page() { return _page; };
  double X() { return _x; };
  double Y() { return _y; };
  double dX() { return _dx; };
  double dY() { return _dy; };
  double fontSize() { return _fontSize; };
  GBool bold() { return _bold; };
  GBool italics() { return _italics; };

  private:
  int _page;
  double _x,_y,_dx,_dy,_fontSize;
  GBool _bold,_italics;

};

enum AltLineType {
  Header,
  Body,
  Footnote,
  Repeat,
  Unknown,
  null,
  PStart,
  Paragraph,
  PEnd,
  BlockQuote
};

class AltLine {
  public:
  AltLine();
  ~AltLine() { delete s; };

  GooList *strings() { return s; };
  void add(AltString *str);
  GBool looksLikeFootnote();

  // basic accessors
  AltLineType type() { return _type; };
  void setType(AltLineType type) { _type = type; };
  int page() { return _page; };
  double X() { return _x; };
  double Y() { return _y; };
  double rightX() { return _x + _dx; };
  double length() { return _dx; };
  double height() { return _dy; };
  double fontSize() { return _fontSize; };
  int chars() { return _chars; };

  private:
  GooList *s;
  int _page, _chars;
  double _x,_y,_dx,_dy,_fontSize;
  AltLineType _type;

};

static int sorter(const void *ptr1, const void *ptr2) {
  AltString *s1;
  AltString *s2;
  s1 = *((AltString**) ptr1);
  s2 = *((AltString**) ptr2);
  return s1->cmpPYX(s2->page(),s2->Y(),s2->X());
};

//------------------------------------------------------------------------
// AltlawDoc
//------------------------------------------------------------------------

class AltlawDoc: public OutputDev {
public:

  AltlawDoc(PDFDoc *pdf);
  virtual ~AltlawDoc();

  void sort() { strings.sort(sorter); }
  AltString* getStringAt(int page, double y, double x,
  		double fudgeY = ALTFUDGEY,
		double fudgeX = ALTFUDGEX );
  GBool isRepeatedStr( AltString *str );
  GBool isRepeatedLine( AltLine *line );
  GBool looksLikeBody( AltLine *line);
  GBool onLeftMargin( AltLine *line );
  GBool onRightMargin( AltLine *line );
  GBool onTabStop( AltLine *line, int tab );
  void print();
  void parse();
  int pages() { return _pages; };

  // Check if file was successfully created.
  virtual GBool isOk() { return gTrue; }

  //---- get info about output device

  // Does this device use upside-down coordinates?
  // (Upside-down means (0,0) is the top left corner of the page.)
  virtual GBool upsideDown() { return gTrue; }

  // Does this device use drawChar() or drawString()?
  virtual GBool useDrawChar() { return gFalse; }

  // Does this device use beginType3Char/endType3Char?  Otherwise,
  // text in Type 3 fonts will be drawn with drawChar/drawString.
  virtual GBool interpretType3Chars() { return gFalse; }

  // Does this device need non-text content?
  virtual GBool needNonText() { return gFalse; }

  // Start a page.
  virtual void startPage(int pg, GfxState *state);

  // End a page.
  virtual void endPage();

  //----- update text state
  virtual void updateFont(GfxState *state);
  virtual void updateTextMat(GfxState *state) { /* printf("updateTextMat\n"); */ }
  virtual void updateCharSpace(GfxState *state) { /* printf("updateCharSpace(%f)\n",state->getCharSpace()); */ }
  virtual void updateRender(GfxState *state) { printf("updateRender\n"); }
  virtual void updateRise(GfxState *state) { printf("updateRise\n"); }
  virtual void updateWordSpace(GfxState *state) { /* printf("updateWordSpace(%f)\n",state->getWordSpace()); */ }
  virtual void updateHorizScaling(GfxState *state) { printf("updateHorizScaling\n"); }
  virtual void updateTextPos(GfxState *state) { /* printf("->(%f,%f)\n",state->getCurY(),state->getCurX()); */ }
  virtual void updateTextShift(GfxState *state, double shift) { /* printf("updateTextShift(%f)\n",shift / -100.0); */ }

  //----- text drawing
  virtual void beginString(GfxState *state, GooString *s);
  virtual void endString(GfxState *state);
  virtual void drawString(GfxState *state, GooString *s);
  virtual void drawChar(GfxState *state, double x, double y,
			double dx, double dy,
			double originX, double originY,
			CharCode code, int nBytes, Unicode *u, int uLen);
  
  virtual void drawImageMask(GfxState *state, Object *ref, 
			     Stream *str,
			     int width, int height, GBool invert,
			     GBool inlineImg);
  virtual void drawImage(GfxState *state, Object *ref, Stream *str,
			  int width, int height, GfxImageColorMap *colorMap,
			 int *maskColors, GBool inlineImg);

private:
  GooList strings;		// list of all strings we get
  GooList lines;		// grouped into lines
  GooList header;
  GooList body;
  GooList footnotes;

  int pageNum;
  int _pages;
  double min_x;
  double max_x;
  int max_chars;

};

#endif
