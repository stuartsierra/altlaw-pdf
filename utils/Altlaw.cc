//=====================
//
// Altlaw.cc
//
//=====================

#ifdef __GNUC__
#pragma implementation
#endif

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <ctype.h>
#include <math.h>
#include "goo/GooString.h"
#include "goo/GooList.h"
#include "GfxState.h"
#include "Page.h"
#include "PDFDoc.h"
#include "GlobalParams.h"
#include "Altlaw.h"

#define maxUnderlineWidth 3.0
//------------------------------------------------------------------------
// AltlawDoc
//------------------------------------------------------------------------

AltlawDoc::AltlawDoc(PDFDoc *pdf) {
  pageNum = -1;
  _pages = 0;
  _pageHeight = _pageWidth = 0.0;

  // let's get down to bizness
  pdf->displayPages(this, 1, pdf->getNumPages(), 100, 100, 0,gTrue, gFalse, gFalse);

  this->parse();

}

AltlawDoc::~AltlawDoc() { }

void AltlawDoc::startPage(int pg, GfxState *state) {
  pageNum = pg;
  // in case we process pages in a random order,
  // we'll just set _pages to the largest page num processed
  _pages = pageNum > _pages ? pageNum : _pages;
  //printf("AltlawDoc::startPage(%d, state);\n",pageNum);
  if(!_pageHeight) _pageHeight = state->getPageHeight();
  if(!_pageWidth)  _pageWidth  = state->getPageWidth();
} 


void AltlawDoc::endPage() {
  pageNum = -1;
  //printf("AltlawDoc::endPage();\n");
}

void AltlawDoc::updateFont(GfxState *state) {
  //printf("AltlawDoc::updateFont(%f);\n",state->getFontSize());
}

void AltlawDoc::beginString(GfxState *state, GooString *s) {
  //printf("AltlawDoc::beginString(state,str);\n");
}

void AltlawDoc::endString(GfxState *state) {
  //printf("AltlawDoc::endString(state);\n");
}

AltString::AltString(GooString *s, int page,
			double x1, double y1, double x2, double y2, double yDraw,
			GfxFont *font, double fontSize, GBool bold, GBool italics) : GooString(s) {
  _page = page;
  _x1 = x1;
  _y1 = y1;
  _x2 = x2;
  _y2 = y2;
  _yDraw = yDraw;
  _font = font;
  _fontSize = fontSize;
  _bold = bold;
  _italics = italics;
  _underline = gFalse;
  _overlap = 0.0;
}

AltString::AltString(AltString *s) : GooString(s) {
  _page = s->_page;
  _x1 = s->_x1;
  _y1 = s->_y1;
  _x2 = s->_x2;
  _y2 = s->_y2;
  _yDraw = s->_yDraw;
  _font = s->_font;
  _fontSize = s->_fontSize;
  _bold = s->_bold;
  _italics = s->_italics;
  _underline = s->_underline;
  _overlap = 0.0;
}

AltWord::AltWord(double x1, double y1, double x2, double y2) : AltString() {
  _x1 = x1;
  _y1 = y1;
  _x2 = x2;
  _y2 = y2;

  // this is arbitrary, but we'll set yDraw just to be complete
  _yDraw = y1;

  sup = _bold = _italics = _underline = gFalse;
  numChars = -1;
}

AltWord::AltWord(AltString *s) : AltString(s) {
  sup = gFalse;
  numChars = -1;

}

AltLine::AltLine(AltlawDoc *doc) {
  _doc = doc;
  _strings = new GooList();
  _words = new GooList();
  _x = _y = _dx = _dy = _fontSize = 0.0;
  _page = -1;
  _type = null;
  _chars = 0;

  capMode = CAPS_UNKNOWN;
  avgWordHeight = 0.0;
  avgCharWidth = 0.0;
  normAvgWordHeight = 0.0;
  normAvgCharWidth = 0.0;
  align = 0.0;
  vertPos = 0.0;
  pageLineId = 0;
  docLineId = 0;
  pageLines = 0;
  docLines = 0;
  prevDistance = 0.0;
  nextDistance = 0.0;
  wordCount = 0;

}

// Add another string to the end of the line
void AltLine::add(AltString *str) {
  // add it to the list
  _strings->append(str);

  // update our line variables
  _chars += str->getLength();
  _dx += str->dX(); // this isn't going to be completely accurate with implied spaces

  // use the largest fontSize / Y val of all strings (perhaps the mode/median/mean would be better?)
  _y = str->Y() > _y ? str->Y() : _y;
  _fontSize = str->fontSize() > _fontSize ? str->fontSize() : _fontSize;

  // set line vals with first string
  if (_strings->getLength() == 1) {
    _page = str->page();
    _x = str->X();
    _y = str->Y();
    _fontSize = str->fontSize();
  }

}

int max(int a, int b) { return (a>b)?a:b; }
double max(double a, double b) { return (a>b)?a:b; }

int max(int vals[], int size) {
  int _max = 0;
  for(int i=0; i<size; i++)
    if( vals[_max] < vals[i] )
      _max = i;

  return _max;
}

double max(double vals[], int size) {
  int _max = 0;
  for(int i=0; i<size; i++)
    if( vals[_max] < vals[i] )
      _max = i;

  return _max;
}

int min(int a, int b) { return (a<b)?a:b; }
double min(double a, double b) { return (a<b)?a:b; }

char* AltString::_lower = "abcdefghijklmnopqrstuvwxyz";
char* AltString::_upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

GBool AltString::hasAlpha() {
  if (strpbrk(this->getCString(), _lower) or strpbrk(this->getCString(), _upper))
    return gTrue;
  else
    return gFalse;
}

GBool AltString::isUpper() {
  if (strpbrk(this->getCString(), _upper) and !strpbrk(this->getCString(), _lower))
    return gTrue;
  else
    return gFalse;
}

GBool AltString::isLower() {
  if (strpbrk(this->getCString(), _lower) and !strpbrk(this->getCString(), _upper))
    return gTrue;
  else
    return gFalse;
}

GBool AltString::isFirstCap() {
  GooString *w = this->copy();
  char *upp = strpbrk(this->getCString(), _upper);
  char *low = strpbrk(this->getCString(), _lower);

  if(upp and (!low or upp < low))
    return gTrue;
  else
    return gFalse;
}

CapMode AltString::capMode() {

    // no alphas
    if (!this->hasAlpha())
      return CAPS_NOALPHA;

    // all caps
    else if (this->isUpper())
      return CAPS_UPPER;

    // no caps
    else if (this->isLower())
      return CAPS_LOWER;

    // first caps
    else if (this->isFirstCap())
      return CAPS_FIRST;

    // caps in the middle ??
    else {
      //printf("WeirdCaps: '%s'\n",this->getCString());
      return CAPS_UNKNOWN; // skip this one
    }


}

void AltLine::calcFeatures() {

  // word-based features
  int _caps[NUM_CAPS_TYPES] = {0};
  double _wh = 0.0; // word height
  double _cw = 0.0; // char width
  int _chars = 0;   // number of chars
  for(int i=0; i<_words->getLength(); i++) {
    AltWord *w = (AltWord *)_words->get(i);

    _caps[w->capMode()]++;
    _wh += w->fontSize();
    _cw += w->_x2 - w->_x1;
    _chars += w->getLength();
  }

  wordCount = _words->getLength();

  capMode = (CapMode) max(_caps, 4);

  avgWordHeight = _wh / (double) (wordCount ? wordCount : 1);
  avgCharWidth  = _cw / (double) (_chars ? _chars : 1);

  if (wordCount) {
    this->_x = ((AltWord *)(_words->get(0)))->_x1;
    this->_dx = ((AltWord *)(_words->get(wordCount - 1)))->_x2 - this->_x;
  } else {
    this->_x = 0.0;
    this->_dx = 0.0;
  }

  align = this->X() / (_doc->pageWidth() - this->rightX());
  vertPos = this->Y() / _doc->pageHeight();


/*
  double normAvgWordHeight; // wait for all lines
  double normAvgCharWidth;  // wait for all lines
  int lineId;
  double prevDistance;
  double nextDistance;
*/

}

void AltlawDoc::normalizeLineFeatures() {

  double pageAvgWordHeight = 0.0;
  double pageAvgCharWidth = 0.0;
  double pageAvgLength = 0.0;
  
  int pageLines = 0;
  int curPage = 1;
  int first = 0; // index to the first line of the current page
  for(int i=0; i<=lines.getLength(); i++) {
    AltLine *line = (i < lines.getLength()) ? (AltLine*)lines.get(i) : NULL;
    if (line == NULL or line->page() != curPage) {
      // go back and update all the lines on this page
      if (pageLines > 0) {
	pageAvgWordHeight /= (double) (pageLines);
	pageAvgCharWidth  /= (double) (pageLines);
	pageAvgLength     /= (double) (pageLines);
      }

      for(int j=first; j<i; j++) {
	AltLine *l = (AltLine*)lines.get(j);
	if (pageAvgWordHeight > 0.0) l->normAvgWordHeight = l->avgWordHeight / (pageAvgWordHeight);
	if (pageAvgCharWidth  > 0.0) l->normAvgCharWidth  = l->avgCharWidth  / (pageAvgCharWidth);
	if (pageAvgLength     > 0.0) l->normLength	= l->length()      / (pageAvgLength);
	l->pageLineId = 1 + j - first;
	l->pageLines = pageLines;
	l->docLineId = 1 + j;
	l->docLines = lines.getLength();


	if (j>first)
	  l->prevDistance = l->Y() - ((AltLine*)(lines.get(j-1)))->Y();
	else
	  l->prevDistance = 0.0;

	if (j<i-1)
	  l->nextDistance = ((AltLine*)(lines.get(j+1)))->Y() - l->Y();
	else
	  l->nextDistance = 0.0;

      }
      first = i;
      pageLines = 0;
      curPage = line ? line->page() : -1;
      pageAvgWordHeight = pageAvgCharWidth = pageAvgLength = 0.0;
    }

    pageLines++;
    pageAvgWordHeight += line ? line->avgWordHeight : 0.0;
    pageAvgCharWidth  += line ? line->avgCharWidth  : 0.0;
    pageAvgLength     += line ? line->length()      : 0.0;
  }
}

GBool AltLine::match(char *str) {
  GooString *check = new GooString(str);
  check->lowerCase();
  for(int i=0; i<_words->getLength(); i++) {
    AltWord *w = (AltWord *)(_words->get(i));
    if (check->cmpN(w->copy()->lowerCase(),check->getLength()) == 0)
      return gTrue;
  }
  return gFalse;
}

int AltlawDoc::findOpinionStart() {

  // Judge or Per Curiam, followed by body line is the best bet
  for(int i=0; i<lines.getLength(); i++) {
    AltLine *l = (AltLine *)(lines.get(i));
    if (!l or l->page() > 2 or i == lines.getLength() - 1) break;
    AltLine *next = (AltLine *)(lines.get(i+1));
    if (l->match("Judge") or (l->match("Per") and l->match("Curiam")))
      if(this->looksLikeBody(l))
	return i;
      else if(this->looksLikeBody(next))
	return i+1;
      else if(next and next->type() == Separator)
	return i;
  }
  // second best is Opinion or Order followed by body line
  for(int i=0; i<lines.getLength(); i++) {
    AltLine *l = (AltLine *)(lines.get(i));
    if (!l or l->page() > 2 or i == lines.getLength() - 1) break;
    AltLine *next = (AltLine *)(lines.get(i+1));
    if (l->match("Opinion") or l->match("Order"))
      if(this->looksLikeBody(l))
	return i;
      else if(this->looksLikeBody(next))
	return i+1;
      else if(next and next->type() == Separator)
	return i;
  }
  return -1;
}

void AltlawDoc::calcLineTypes() {

  // Try to figure out line types
  AltLine *prev, *line, *next;
  prev = line = next = NULL;

  int n = this->findOpinionStart();
  /*
  if(n >= 0) {
    printf("<!-- Opinion Starts at line %d:\n",n);
    AltLine *op = (AltLine *)(lines.get(n));
    op->print(gTrue);
    printf("-->\n");
  } else
    printf("<!-- Opinion Starts at - NOT FOUND -->\n");
  */

  GBool inHeader = gTrue;
  for(int i=0; i<lines.getLength(); i++) {
    if (line and line->type() != Repeat) prev = line;
    line = (AltLine *)lines.get(i);

    if ( i < n ) {
      if (line->type() == null ) line->setType(Header);
      header.append(line);
    }

    // maybe we've already figured this one out?
    // we should check if this separator is for footnotes
    else if ( line->type() == Separator ) continue; //body.append(line);

    // Look for Repeats in first and last 2 lines
    else if ( min( line->pageLineId, line->pageLines - line->pageLineId ) <= 2
	 and this->isRepeatedLine(line) ) {
      line->setType(Repeat);
      repeats.append(line);
    }

    // Footnotes
    else if ( line->looksLikeFootnote()
	 or ( prev and prev->type() == Separator and this->onLeftMargin(prev) and line->page() == prev->page())) {
      line->setType(Footnote);
      footnotes.append(line);
    }

    // Once we find a footnote, pretty much everything below it
    // is also a footnote
    else if ( prev and prev->type() == Footnote and line->page() == prev->page()) {
      line->setType(Footnote);
      footnotes.append(line);
    }

  }

  prev = next = NULL;
  for(int i=0; i<lines.getLength(); i++) {

    // next and prev pointers should skip Repeat/Separator/Footnote lines
    if (line and
      line->type() != Repeat and
      line->type() != Separator and
      line->type() != Footnote ) prev = line;

    line = (AltLine *)lines.get(i);

    // next should point to the next non-Repeat, non-Separator line
    int j=1;
    while (i+j+1 < lines.getLength()) {
      next = (AltLine *)lines.get(i+j);
      if (next and
	next->type() != Repeat and
	next->type() != Separator and
	next->type() != Footnote ) break;
      j++;
    }
    if (i+j+1 >= lines.getLength()) next = NULL;

    // Parse Body sections
    if (line->type() != null) continue;
    else if (line->looksLikeSectionHeading())
      line->setType(SectionHeading);
    else if (this->onLeftMargin(line)) {
      if (prev and prev->type() == BQend) {
        if (!this->onRightMargin(line))
          line->setType(Psingle);
        else if (prev->type() == BQend)
          line->setType(Pstart);
      }
      else if (prev and (prev->type() == Paragraph or prev->type() == Pstart)) {
        if (this->onRightMargin(line) and next and this->onLeftMargin(next) and not next->looksLikeSectionHeading())
          line->setType(Paragraph);
	else
          line->setType(Pend);
      }
    }
    else {
      if (prev and (prev->type() == BlockQuote or prev->type() == BQstart) and fabs(line->X() - prev->X()) < 0.5) {
	if (!next or fabs(next->X() - line->X()) > 0.5)
	  line->setType(BQend);
	else
	  line->setType(BlockQuote);
      }
      else if (next and fabs(line->X() - next->X()) < 0.5)
	line->setType(BQstart);
      else if (/*this->onTabStop(line,1) and */this->onRightMargin(line))
	line->setType(Pstart);
    }

    body.append(line);
  }

}

void AltlawDoc::calcFeatures() {

  // right margin
  GooList *xlist = new GooList();

  for(int i=0; i<lines.getLength(); i++)
    xlist->append(new AltDataPoint(((AltLine *)(lines.get(i)))->rightX()));

  xlist->sort(DataCmp);

  // Let's use the Median (equal number of values higher and lower)
  _rightMargin = ((AltDataPoint *)(xlist->get(xlist->getLength() / 2)))->data;
  //printf("len: %d, median: %d\n",xlist->getLength(), xlist->getLength() / 2);
  //printf("right margin: %.1f\n",_rightMargin);

  // ok, now left margin
  deleteGooList(xlist, AltDataPoint);
  xlist = new GooList();

  for(int i=0; i<lines.getLength(); i++)
    xlist->append(new AltDataPoint(((AltLine *)(lines.get(i)))->X()));

  xlist->sort(DataCmp);

  // Let's use the Mode (value that occurs the most)
  AltDataPoint *keeper = (AltDataPoint *)(xlist->get(xlist->getLength() - 1));
  for(int i=xlist->getLength() - 2; i>=0; i--) { // start with second to last and work forwards
    AltDataPoint *d = (AltDataPoint *)(xlist->get(i));

    if (DataCmp(&keeper,&d) == 0) {
      keeper->count++;
      xlist->del(i);
    } else { keeper = d; }

  }
  xlist->sort(CountCmp);

  // this sets the LeftMargin to the Mode of all line X vals
  _leftMargin = ((AltDataPoint *)(xlist->get(xlist->getLength() - 1)))->data;
  //printf("left margin: %.1f\n",_leftMargin);

  GooList *ylist = new GooList();
  AltLine* lastline = (AltLine *)(lines.get(0));
  for(int i=1; i<lines.getLength(); i++) {
    AltLine* l = (AltLine *)(lines.get(i));
    ylist->append(new AltDataPoint(l->Y() - lastline->Y()));
    lastline = l;
  }

  ylist->sort(DataCmp);

  keeper = (AltDataPoint *)(ylist->get(ylist->getLength() - 1));
  for(int i=ylist->getLength() - 2; i>0; i--) { // start with second to last and work forwards
    AltDataPoint *d = (AltDataPoint *)(ylist->get(i));

    if (DataCmp(&keeper,&d) == 0) {
      keeper->count++;
      ylist->del(i);
    } else { keeper = d; }
  }

  ylist->sort(CountCmp);

}

void AltLine::chomp() {
  // chop the last word if it's still empty
  int n = _words->getLength();
  if (n and ((AltWord *)(_words->get(n-1)))->getLength() == 0)
    _words->del(n-1);
}

AltWord* AltLine::startNewWord(AltString *s) {
  this->chomp();
  AltWord *w = new AltWord(s);
  w->_line = this;
  w->clear();
  _words->append(w);
  return w;

}

void AltLine::parseWords() {
  deleteGooList(_words, AltWord);
  _words = new GooList();
  AltString *last = NULL;
  AltWord *w = NULL;

  // assume _strings is sorted
  for(int i=0; i<_strings->getLength(); i++) {
    AltString *s = (AltString *)(_strings->get(i));
    if (i == 0) w = this->startNewWord(s);

    // if there is a large space, start a new word
    if (i > 0 && fabs(s->_x1 - last->_x2) > 1.0 ) {
      w = this->startNewWord(s);
    }

    // if it looks different, start a new word
    if ( not w->looksSimilar(s) )
      w = this->startNewWord(s);

    if(s->looksLikeFootnote(this->Y(), this->fontSize()))
      w->sup = gTrue;

    char *str = s->getCString();
    
    // if we find a space, start a new word
    while(char *pch = strchr(str,' ')) {

      // calculate delta X
      // HACK - b/c I don't feel like using getNextChar
      double dx = s->dX() * ((double)(pch - s->getCString()) / (double)(s->getLength()));

      if (pch-str) {
	// append pre-space string
	w->append(str,pch-str);
	// add delta to existing word if the space is mid-string
	w->_x2 = s->_x1 + dx;
      }


      // start new word
      w = this->startNewWord(s);
      w->_x1 = s->_x1 + dx;
      w->_x2 = s->_x2;

      // advance pointer to look for next space
      str = pch+1;
    }

    // ok, now add the (remaining) string to our word
    w->append(str);
    w->_x2 = s->_x2;
    last = s;

  }

  // handle last string in list
  this->chomp();

}

GBool AltlawDoc::looksLikeBody(AltLine *line) {
  if (!line) return gFalse;

  if (this->onRightMargin(line) and line->X() < ((_rightMargin + _leftMargin) / 2)) return gTrue;
  else return gFalse;

  if (this->onLeftMargin(line) and this->onRightMargin(line)) return gTrue;
  if (this->onTabStop(line,1) and this->onRightMargin(line)) return gTrue;

  // just in case tab is not = 50.0, which is hardcoded in onTabStop right now
  if (line->X() - _leftMargin < 100.0 and this->onRightMargin(line)) return gTrue;
  return gFalse;

 }

GBool AltLine::isBold() {
  // currently require all words to be bold
  // should change to majority bold...
  for(int i=0; i<_words->getLength(); i++)
    if(!((AltWord *)(_words->get(i)))->bold())
      return gFalse;

  return gTrue;

}

GBool AltLine::looksLikeSectionHeading() {
  if (this->isBold())
    return gTrue;
  else if (this->capMode == CAPS_UPPER)
    return gTrue;
  else if (this->normLength < 0.25 and fabs(1.0 - align) < 0.10 )
    return gTrue;
  else
    return gFalse;
}

GBool AltString::looksSimilar(AltString *s) {

  if (this->_bold != s->_bold)
    return gFalse;

  if (this->_italics != s->_italics)
    return gFalse;

  if (this->_underline != s->_underline)
    return gFalse;

  double deltaAllowed = 0.25;
  if (this->capMode() != s->capMode())
    deltaAllowed = 0.25;
  else
    deltaAllowed = 0.50;

  if (fabs(this->_fontSize - s->_fontSize)/max(this->_fontSize,s->_fontSize) > deltaAllowed)
    return gFalse;

  if ( fabs((this->_y2 - this->_y1) - (s->_y2 - s->_y1))/max((this->_y2 - this->_y1) , (s->_y2 - s->_y1)) > deltaAllowed) 
    return gFalse;

  if (this->_font != s->_font)
    return gFalse;

  // We made it!
  return gTrue;

}

GBool AltString::looksLikeFootnote(double lineY, double lineFont) {
  int n = 0;
  char c = this->getChar(0);
  if (c >= '0' and c <= '9') n+=3;
  if (c == '*') n+=3;
  if (this->Y() < lineY) n++;
  if (this->fontSize() < lineFont) n++;
  if (this->bold()) n++;
  if (this->italics()) n++;
  if (n >= 5) return gTrue;
  else return gFalse;
}

// Should this be in AltlawDoc:: ?
GBool AltLine::looksLikeFootnote() {
  return ((AltString *)(_strings->get(0)))->looksLikeFootnote(this->Y(), this->fontSize());
}

void AltlawDoc::drawString(GfxState *state, GooString *s) {
  GfxFont *font;
  CharCode code;
  Unicode u[8];
  double x, y, tx, ty, dx, dx_char, tdx, yMin, yMax;
  double dy, tdy, ox, oy; /* these are useless unless in vertical mode */
  char *p;
  int len, n, uLen, nChars, nSpaces, i;

  font = state->getFont();

  char *fontname = font->getName()->lowerCase()->getCString();
  GBool bold = font->isBold() or strstr(fontname,"bold");
  GBool italic = font->isItalic() or strstr(fontname,"italic") or strstr(fontname,"oblique");

  dx = dy = 0;
  p = s->getCString();
  len = s->getLength();
  nChars = nSpaces = 0;
  // calc raw dx, chars, and spaces
  while (len > 0) {
    n = font->getNextChar(p, len, &code,
			    u, (int)(sizeof(u) / sizeof(Unicode)), &uLen,
			    &dx_char, &dy, &ox, &oy);
    dx += dx_char;
    if (n == 1 && *p == ' ') {
	++nSpaces;
    }
    ++nChars;
    p += n;
    len -= n;
  }

  // apply font and text params
  dx = dx * state->getFontSize()
	   + nChars * state->getCharSpace()
	   + nSpaces * state->getWordSpace();
  dx *= state->getHorizScaling();

  // apply Transform Matrix
  state->textTransformDelta(dx, dy, &tdx, &tdy);
  state->transformDelta(tdx,tdy,&dx,&dy);
  state->transform(state->getCurX(),state->getCurY(),&tx,&ty);
  //state->transform(state->getCurY() - (font->getAscent() * state->getFontSize()),
  //		state->getCurY() - (font->getDescent() * state->getFontSize()),
  //		&yMin,&yMax);
  yMin = ty - font->getAscent()  * state->getTransformedFontSize();
  yMax = ty - font->getDescent() * state->getTransformedFontSize();

  strings.append(new AltString(s,pageNum,
  				tx,yMin, tx+dx,yMax, ty,
  				font,state->getTransformedFontSize(),bold,italic));
}

// Figure out where the string is in relation to the page/y/x coords given
int AltString::cmpPYX(int page, double x1, double y1, double x2, double y2) {

  return
    _page > page ?  1 :
    _page < page ? -1 :

    this->yOverlap(y1,y2) > 0.33 * min(this->_y2 - this->_y1, y2 - y1) ? (
      _x1 >= x2 ?  1 :
      _x2 <= x1 ? -1 :
    0 ) :

    (y2 - _y1) < (_y2 - y1) ? 1 : -1;
}

void AltlawDoc::stroke(GfxState *state) {
  // Adapted from TextOutputDev.cc
  GfxPath *path;
  GfxSubpath *subpath;
  double x[2], y[2], t;
  int i;

  path = state->getPath();
  if (path->getNumSubpaths() != 1) {
    printf("<!-- Ignoring Multi-Path Stroke -->\n");
    return;
  }
  subpath = path->getSubpath(0);
  if (subpath->getNumPoints() != 2) {
    printf("<!-- Ignoring Multi-Point Stroke -->\n");
    return;
  }
  for (i = 0; i < 2; ++i) {
    if (subpath->getCurve(i)) {
      printf("<!-- Ignoring Curve Stroke -->\n");
      return;
    }
    state->transform(subpath->getX(i), subpath->getY(i), &x[i], &y[i]);
  }

  if (x[1] < x[0]) {
    t = x[0];
    x[0] = x[1];
    x[1] = t;
  }
  if (y[1] < y[0]) {
    t = y[0];
    y[0] = y[1];
    y[1] = t;
  }

  // skinny horizontal rectangle
  this->addUnderline(this->pageNum, x[0], y[0], x[1], y[0]);
}

void AltlawDoc::fill(GfxState *state) {
  // Code from TextOutputDev.cc
  GfxPath *path;
  GfxSubpath *subpath;
  double x[5], y[5];
  double rx0, ry0, rx1, ry1, t;
  int i;

  path = state->getPath();
  if (path->getNumSubpaths() != 1) {
    //printf("<!-- Ignoring Multi-Path Fill -->\n");
    return;
  }
  subpath = path->getSubpath(0);
  if (subpath->getNumPoints() != 5) {
    //printf("<!-- Ignoring Multi-Point / Non-Rectangle Fill -->\n");
    return;
  }
  for (i = 0; i < 5; ++i) {
    if (subpath->getCurve(i)) {
      return;
    }
    state->transform(subpath->getX(i), subpath->getY(i), &x[i], &y[i]);
  }

  // look for a rectangle
  if (x[0] == x[1] && y[1] == y[2] && x[2] == x[3] && y[3] == y[4] &&
      x[0] == x[4] && y[0] == y[4]) { // first subpath vertical
    rx0 = x[0];
    ry0 = y[0];
    rx1 = x[2];
    ry1 = y[1];
  } else if (y[0] == y[1] && x[1] == x[2] && y[2] == y[3] && x[3] == x[4] &&
	     x[0] == x[4] && y[0] == y[4]) { // first subpath horizontal
    rx0 = x[0];
    ry0 = y[0];
    rx1 = x[1];
    ry1 = y[2];
  } else {
    //printf("<!-- Ignoring Non-Rectangle Fill -->\n");
    return;
  }
  if (rx1 < rx0) {
    t = rx0;
    rx0 = rx1;
    rx1 = t;
  }
  if (ry1 < ry0) {
    t = ry0;
    ry0 = ry1;
    ry1 = t;
  }

  //printf("<RECTANGLE x1=%.0f y1=%.0f x2=%.0f y2=%.0f />\n",rx0,ry0,rx1,ry1);

  // skinny horizontal rectangle
  if (ry1 - ry0 < rx1 - rx0) {
    if (ry1 - ry0 < maxUnderlineWidth) {
      ry0 = 0.5 * (ry0 + ry1); // use the average y (mean)
      this->addUnderline(this->pageNum, rx0, ry0, rx1, ry0);
    }

  // skinny vertical rectangle
  } else {
    /*
    if (rx1 - rx0 < maxUnderlineWidth) {
      rx0 = 0.5 * (rx0 + rx1);
      text->addUnderline(rx0, ry0, rx0, ry1);
    }
    */
    //printf("<!-- Ignoring Rectangle Fill -->\n");
  }
}

void AltlawDoc::eoFill(GfxState *state) {
  this->fill(state);
}

void AltlawDoc::addUnderline(int page, double x1, double y1, double x2, double y2) {
  AltString *ul = new AltString( new GooString("========="), page, x1, y1, x2, y2, (y1 + y2) / 2.0, NULL, 0.0, gFalse, gFalse);
  ul->_underline = gTrue;
  this->underlines.append(ul);
}

GBool AltString::isNum() {
  GBool res = gFalse;
  for(int n=0; n<this->getLength(); n++) {
    char c = this->getChar(n);
    // ignore these chars
    if (c == ' ') continue;
    if (c == '-') continue;
    if (c == '.') continue;

    if (c >= '0' and c <= '9')
      res = gTrue;
    else
      return res; // we're done b/c we found non-digit

  }
  return res;
}

double AltString::yOverlap(double y1, double y2) {

  double max_y1 = max(_y1, y1);
  double min_y2 = min(_y2, y2);

  // special case - lines get 1.0 overlap
  if ((max_y1 == _y1 and min_y2 == _y2 ) or
      (max_y1 == y1  and min_y2 == y2  ))
    return max(1.0, min_y2 - max_y1);
  else
    return min_y2 - max_y1;
}

AltWord* AltlawDoc::getWordAt(int page, double x1, double y1, double x2, double y2 ) {
  int p1 = 0;
  int p2 = words.getLength() - 1;
  int guess;
  if (p2 <= 0) {
    printf("Error: can't getWordAt w/ No Words!\n");
    return NULL;
  }
  AltWord *word = NULL;
  while (p1 <= p2) {
    guess = (p1 + p2) / 2;
    word = (AltWord *)(words.get(guess));
    int check = word->cmpPYX(page,x1,y1,x2,y2);
    if (check == 0)
      return word;
    else if (check < 0)
      p1 = guess + 1;
    else
      p2 = guess - 1; 
  }
  return NULL; // not found       
}

GBool AltlawDoc::isRepeatedWord(AltWord *word) {

    // search each page - could try to mark the "hits"
    // (so we don't have to re-check them)
    // but we don't
    int hits = 0;
    AltWord *repeat = NULL;

    for(int p=1; p<=this->pages(); p++) {
      if (p == word->page()) continue;
      
      // look for a string at the same coords on this page
      repeat = this->getWordAt(p, word->X(), word->yMin(), word->X() + word->dX(), word->yMax() );

      // found one?
      if (repeat and (word->cmp((GooString *)repeat) == 0 or word->isNum() and repeat->isNum()) )
	hits++;
    }

    // need to rethink the strategy here, perhaps
    if (hits > 0)
      return gTrue;
    else
      return gFalse;
}

GBool AltlawDoc::isRepeatedLine(AltLine *line) {

    // Look for repeated lines - we want to ignore these (headers/footers/etc)
    int hits = 0;
    for(int i=0; i < line->words()->getLength(); i++) {
      AltWord *word = (AltWord *)(line->words()->get(i));

      // bail if we find any non-repeats
      if (this->isRepeatedWord(word))
	hits++;
    }
    // all strings in line are repeats
    if (hits > (int)(0.5 * line->words()->getLength()) )
      return gTrue;
    //else if (hits > 0) { printf("hits: %d <= %d\n",hits, (int)(0.5 * line->words()->getLength())); return gFalse; }
    else
      return gFalse;
}

// print out nice html
void printCString(char *s, int len) {

  for(int i=0; i<len; i++) {
    CharCode c = (CharCode)(*(s+i) & 0xff);

    // check for non ascii chars
    if (c > 0x7F) {
      switch(c) {
	case(0x91):
	case(0x92):
	  printf("'");
	  break;
	case(0x93):
	case(0x94):
	case(0xAA):
	case(0xBA):
	  printf("\"");
	  break;
	case(0x96):
	case(0x97):
	case(0xAD):
	case(0xD0):
	  printf("-");
	  break;
	case(0xA7):
	  printf("&sect;");
	  break;
	case(0xB6):
	  printf("&para;");
	  break;
	case(0xBD):
	  printf("+");
	  break;
	case(0xFC):
	case(0xFD):
	case(0xFE):
	  // ignore - these are weird brackets that show up occasionally
	  break;
      }

    } else if (c >= 0x20 ) { printf("%c",c); }
  }
}

void AltString::print(GBool full) {
  if (full) printf("<STRING page=%d x=%.0f,%.0f y=%.0f,%.0f overlap=%.2f>",
		    _page, _x1,_x2, _y1,_y2, _overlap);
  printCString(this->getCString(), this->getLength());
  if (full) printf("</STRING>\n");
}

void AltWord::print(GBool full) {
  if (full) {
    printf("<WORD page=%d x=%.0f,%.0f y=%.0f,%.0f caps=\"%d\" font=\"%.1f\">",
		    _page, _x1,_x2, _y1,_y2,
		    this->capMode(),this->fontSize() );

    if(this->sup) printf("<sup>");
    if(this->_italics) printf("<i>");
    if(this->_bold) printf("<b>");
    if(this->_underline) printf("<u>");
  }

  printCString(this->getCString(), this->getLength());

  if (full) {
    if(this->_underline) printf("</u>");
    if(this->_bold) printf("</b>");
    if(this->_italics) printf("</i>");
    if(this->sup) printf("</sup>");
    printf("</WORD>\n");
  }
}

void AltLine::print(GBool full) {

  if (this->type() == Separator) {
    AltString *sep = (AltString *)(this->strings()->get(0));
    if(full)
      printf("<SEPARATOR page=%d coords=%.0f,%.0f->%.0f,%.0f />\n",sep->_page,sep->_x1,sep->_y1,sep->_x2,sep->_y2);
    else {
      printf("<p class=\"center\">");
      sep->print(gFalse);
      printf("</p>\n");
    }
  }
  else {
    if (full)
      printf("<LINE page=%d x=%.0f,%.0f(%.1f) y=%.0f type=%d caps=%d awh=%.1f(%.2f) acw=%.1f(%.2f) h=%.2f v=%.2f n=%d Pid=%d/%d Did=%d/%d d=%.1f/%.1f>\n",
	_page, _x,_x+_dx, normLength, _y,
	this->type(), this->capMode, this->avgWordHeight, this->normAvgWordHeight,
	this->avgCharWidth, this->normAvgCharWidth, this->align, this->vertPos, this->wordCount,
	this->pageLineId, this->pageLines, this->docLineId, this->docLines, this->prevDistance, this->nextDistance);
    else {
      switch(this->type()) {
	case(Header):
	  printf("<p class=\"indent\">");
	  break;
	case(SectionHeading):
	  printf("<p class=\"center\">");
	  break;
	case(Pstart):
	  printf("<p class=\"indent\">");
	  break;
	case(BQstart):
	  printf("<blockquote><p>");
	case(BlockQuote):
	case(BQend):
	//  for(int i=0; i < (int)((this->X() - this->_doc->leftMargin())/this->fontSize()); i++)
	//    printf("  ");
	  break;
	case(Paragraph):
	case(Pend):
	case(Footnote):
	  break;
	case(Psingle):
	  printf("<p>");
	  break;
	default:
	  printf("<p class=\"indent\">");
	  break;
      } 
    }
    GBool sup, i, b, u;
    sup = i = b = u = gFalse;
    for(int j=0; j < this->words()->getLength(); j++) {
      AltWord *w = (AltWord *)(this->words()->get(j));
      if (!full) {
	if (sup and not w->sup) printf("</sup>");
	if (i and not w->_italics) printf("</i>");
	if (b and not w->_bold) printf("</b>");
	if (u and not w->_underline) printf("</i>"); // use italics instead of underlines

	if (j != 0 and not w->sup) printf(" ");

	if (not u and w->_underline) printf("<i>");
	if (not b and w->_bold) printf("<b>");
	if (not i and w->_italics) printf("<i>");
	if (not sup and w->sup) printf("<sup>");
      }
      sup = w->sup; i = w->_italics; b = w->_bold; u = w->_underline;
      w->print(full);
      if (!full and j == this->words()->getLength() - 1) {
	if(u) printf("</i>"); if(b) printf("</b>"); if(i) printf("</i>"); if(sup) printf("</sup>");
      }
    }

    if (full) printf("</LINE>\n");
    else {
      switch(this->type()) {
	case(Pstart):
	case(BQstart):
	case(Paragraph):
	case(BlockQuote):
	case(Footnote):
	  break;
	case(BQend):
	  printf("</p></blockquote>");
	  break;
	default:
	  printf("</p>");
	  break;
      }
      printf("\n");
    }
  }
}

void AltlawDoc::print(GBool full) {
  //AltString *last = NULL;

  if(full) {
    printf("<!-- STRINGS -->\n");
    for(int i=0; i < strings.getLength(); i++)
      ((AltString *)(strings.get(i)))->print(full);

    printf("<!-- LINES -->\n");
    for(int i=0; i < lines.getLength(); i++)
      ((AltLine *)(lines.get(i)))->print(full);

    printf("<!-- END FULL DATA -->\n");

    // log ignored repeats in a comment
    printf("<!-- REPEATS:\n");
    for(int i=0; i < repeats.getLength(); i++)
      ((AltLine *)(repeats.get(i)))->print(full);
    printf("-->\n");
  }

  // header
  printf("<div class=\"prohtml\">");
  printf("<div class=\"prelims\">");

  for(int i=0; i < header.getLength(); i++)
    ((AltLine *)(header.get(i)))->print(gFalse);
  printf("</div>\n");

  // body
  for(int i=0; i < body.getLength(); i++)
    ((AltLine *)(body.get(i)))->print(gFalse);

  // footnotes
  printf("<div class=\"footnotes\">\n");
  for(int i=0; i < footnotes.getLength(); i++) {
    AltLine *fn   = (AltLine *)(footnotes.get(i));
    AltLine *next = i + 1 < footnotes.getLength() ? (AltLine *)(footnotes.get(i+1)) : NULL;

    // decide when to start a new footnote (all have type = Footnote)
    if (fn->looksLikeFootnote())
      printf("<p id=\"fn%s\">\n",((AltString *)(fn->strings()->get(0)))->getCString());

    fn->print(gFalse);

    if (!next or next->looksLikeFootnote())
      printf("</p>\n");
  }
  printf("</div></div>\n\n");

}

static int dblsrt(const void *ptr1, const void *ptr2) {
  double *d1 = *((double**) ptr1);
  double *d2 = *((double**) ptr2);
  return d1 > d2 ? 1 : d1 == d2 ? 0 : -1; 
}

void AltlawDoc::parse() {

  // Sort strings by Page, Y, X
  this->sort();

  // Group strings into Lines
  double y1 = 0.0;
  double y2 = 0.0;
  AltLine *line = NULL;
  AltString *s = NULL;
  for(int i=0; i<strings.getLength(); i++) {
    s = (AltString *)(strings.get(i));

    // need to modify to deal with close but not equal
    s->_overlap = s->yOverlap(y1,y2);
    if (s->_overlap <= 0.33 * min(s->_y2 - s->_y1, y2 - y1)) {

      // start a new line
      y1 = s->yMin();
      y2 = s->yMax();
      line = new AltLine(this);
      lines.append(line);
    }
    line->add(s);
  }
  for(int i=0; i<lines.getLength(); i++) {
    line = (AltLine *)(lines.get(i));
    line->sort();
    line->parseWords();
    line->calcFeatures();
  }
  this->normalizeLineFeatures();

  // create sorted word list
  // should probably reset the word goolist
  for(int i=0; i<lines.getLength(); i++) {
    line = (AltLine *)(lines.get(i));
    words.append(line->words());
  }

  for(int i=0; i<underlines.getLength(); i++) {
    AltString *ul = (AltString *)(underlines.get(i));
    GBool hit = gFalse;

    AltWord *w = this->getWordAt(ul->_page, ul->_x1, ul->_y1, ul->_x2, ul->_y2);
    if(w)
      for(int i=0; i<w->_line->words()->getLength(); i++) {
	AltWord *checkit = (AltWord *)(w->_line->words()->get(i));
	if (checkit->cmpPYX(ul->_page, ul->_x1, ul->_y1, ul->_x2, ul->_y2) == 0)
	  checkit->_underline = gTrue;
      }

    else {
      AltLine *separator = new AltLine(this);
      separator->add(ul);
      separator->setType(Separator);
      // totally inefficient, but I'm hungry...  will fix later
      //printf("Inserting separator at p=%d, Y=%.0f\n",separator->page(), separator->Y());
      for(int i=0; i<lines.getLength(); i++) {
	AltLine *l = (AltLine *)(lines.get(i));
	if(l->page() > separator->page() or (l->page() == separator->page() and l->Y() > separator->Y())) {
	  lines.insert(i,separator);
	  break;
	}
      }
    }
  }

  // Figure out line spacing / tabs / etc
  this->calcFeatures();
  this->calcLineTypes();
}

GBool AltlawDoc::onLeftMargin(AltLine *line) {
  if (fabs(line->X() - _leftMargin) < 1.0) return gTrue;
  else return gFalse;
}

GBool AltlawDoc::onRightMargin(AltLine *line) {
  if ((_rightMargin - line->rightX()) < (0.15 * (_rightMargin - _leftMargin))) return gTrue;
  else return gFalse;
}

GBool AltlawDoc::onTabStop(AltLine *line, int tab) {
  double tabstop = 50.0;
  if (fabs(line->X() - _leftMargin - (tab * tabstop)) < (0.5 * tabstop)) return gTrue;
  else return gFalse;
}

void AltlawDoc::drawChar(GfxState *state, double x, double y,
	      double dx, double dy,
	      double originX, double originY,
	      CharCode code, int /*nBytes*/, Unicode *u, int uLen) 
{
  printf("<!-- unhandled virtual function call: drawChar(%f,%f)[%d - %d]; -->\n",x,dx,code,uLen);
}

void AltlawDoc::drawImageMask(GfxState *state, Object *ref, Stream *str,
			      int width, int height, GBool invert,
			      GBool inlineImg) {
  printf("<!-- unhandled virtual function call: AltlawDoc::drawImageMask(...); -->\n");
}

void AltlawDoc::drawImage(GfxState *state, Object *ref, Stream *str,
			  int width, int height, GfxImageColorMap *colorMap,
			  int *maskColors, GBool inlineImg) {
  printf("<!-- unhandled virtual function call: AltlawDoc::drawImage(...); -->\n");
}
