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
}

AltWord::AltWord(double x1, double y1, double x2, double y2) : AltString() {
  _x1 = x1;
  _y1 = y1;
  _x2 = x2;
  _y2 = y2;

  // this is arbitrary, but we'll set yDraw just to be complete
  _yDraw = y1;

  sup = _bold = _italics = gFalse;
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

char* AltWord::_lower = "abcdefghijklmnopqrstuvwxyz";
char* AltWord::_upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

GBool AltWord::hasAlpha() {
  if (strpbrk(this->getCString(), _lower) or strpbrk(this->getCString(), _upper))
    return gTrue;
  else
    return gFalse;
}

GBool AltWord::isUpper() {
  if (strpbrk(this->getCString(), _upper) and !strpbrk(this->getCString(), _lower))
    return gTrue;
  else
    return gFalse;
}

GBool AltWord::isLower() {
  if (strpbrk(this->getCString(), _lower) and !strpbrk(this->getCString(), _upper))
    return gTrue;
  else
    return gFalse;
}

GBool AltWord::isFirstCap() {
  GooString *w = this->copy();
  char *upp = strpbrk(this->getCString(), _upper);
  char *low = strpbrk(this->getCString(), _lower);

  if(upp and (!low or upp < low))
    return gTrue;
  else
    return gFalse;
}

CapMode AltWord::capMode() {

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
      printf("WeirdCaps: '%s'\n",this->getCString());
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

  avgWordHeight = _wh / (double) wordCount;
  avgCharWidth  = _cw / (double) _chars;

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

void AltLine::parseWords() {
  deleteGooList(_words, AltWord);
  _words = new GooList();
  AltString *last = NULL;
  AltWord *w = NULL;

  // assume _strings is sorted
  for(int i=0; i<_strings->getLength(); i++) {
    AltString *s = (AltString *)(_strings->get(i));
    if (!w or !w->getLength()) {
      w = new AltWord(s);
      w->clear();
    }

    // if there is a large space, start a new word
    if (i > 0 && fabs(s->_x1 - last->_x2) > 1.0 ) {
      if(w->getLength()) {
        _words->append(w);
        w = new AltWord(s);
        w->clear();
      }
    }

    // if this looks like a footnote, start a new word
    if(s->looksLikeFootnote(this->Y(), this->fontSize())) {
      if(w->getLength()) {
        _words->append(w);
        w = new AltWord(s);
        w->clear();
      }
      w->sup = gTrue;
    }

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
      if(w->getLength()) {
        _words->append(w);
        w = new AltWord(s);
        w->clear();
      }
      w->_x1 = s->_x1 + dx;
      w->_x2 = s->_x2;

      // advance pointer to look for next space
      str = pch+1;
    }

    // ok, now add the (remaining) string to our word
//    w->append(str,s->getLength() - (str - s->getCString()));
    w->append(str);
    w->_x2 = s->_x2;
    last = s;

  }

  // handle last string in list
  if(w->getLength()) _words->append(w);

}

GBool AltlawDoc::looksLikeBody(AltLine *line) {

  if (this->onRightMargin(line) and line->X() < ((rightMargin + leftMargin) / 2)) return gTrue;
  else return gFalse;

  if (this->onLeftMargin(line) and this->onRightMargin(line)) return gTrue;
  if (this->onTabStop(line,1) and this->onRightMargin(line)) return gTrue;

  // just in case tab is not = 50.0, which is hardcoded in onTabStop right now
  if (line->X() - leftMargin < 100.0 and this->onRightMargin(line)) return gTrue;
  return gFalse;

 }

GBool AltLine::isBold() {
  for(int i=0; i<_strings->getLength(); i++) {
    if(!((AltString *)(_strings->get(i)))->bold()) {
      /* if (i > 0)
        printf("not bold: '%s'\n", ((AltString *)(s->get(i)))->getCString());; */
      return gFalse;
    }
  }
  return gTrue;

}

GBool AltLine::looksLikeSectionHeading() {

  if (this->isBold())
    return gTrue;
  else
    return gFalse;

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

  double x_mid = (x1 + x2) / 2.0;

  return
    _page > page ?  1 :
    _page < page ? -1 :

    this->yOverlap(y1,y2) > 0.33 * _fontSize ? (
      _x1 >= x2 ?  1 :
      _x2 <= x1 ? -1 :
    0 ) :

    (y2 - _y1) < (_y2 - y1) ? 1 : -1;
}

// used when looking for repeated strings
GBool AltString::similar(AltString *str) {
  if (
    this->cmp((GooString *)str) == 0 or  // exact match
    this->isNum() and str->isNum()  // numbers.. should probably look deeper
  ) return gTrue;
  else return gFalse;
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
  return gFalse;
}

double AltString::yOverlap(double y1, double y2) {

  double diff1 = y2 - this->_y1;
  double diff2 = this->_y2 - y1;

  return diff1 < diff2 ? diff1 : diff2;

}

AltString* AltlawDoc::getStringAt(int page, double y, double x,
				double fudgeY, double fudgeX ) {
  int p1 = 0;
  int p2 = strings.getLength();
  int guess;
  AltString *str = NULL;
  while (p1 < p2) {
    guess = p1 + (p2 - p1)/2;
    str = (AltString *)(strings.get(guess));
    if (str->cmpPYX(page,x,y,x+fudgeX,y+fudgeY) <= 0 )
      p1 = guess + 1;
    else
      //can't be high = mid-1: here A[mid] >= value,
      //so high can't be < mid if A[mid] == value
      p2 = guess; 
  }

  if (p1 < strings.getLength() and str->cmpPYX(page,x,y,x+fudgeX,y+fudgeY) == 0 ) {
    return str; // found
  }
  else
    return NULL; // not found       
}

GBool AltlawDoc::isRepeatedStr(AltString *str) {

    // search each page - could try to mark the "hits"
    // (so we don't have to re-check them)
    // but we don't
    int hits = 0;
    AltString *repeat = NULL;

    for(int p=1; p<this->pages(); p++) {
      if (p == str->page()) continue;
      
      // look for a string at the same coords on this page
      // use a low Y "fudge" but higher X b/c page numbers
      // tend to be centered in the footer
      // which causes the X coord to shift slightly
      repeat = this->getStringAt(p, str->Y(), str->X(), 0.3, 8.0 );

      // found one?
      if (repeat and str->similar(repeat) )
        hits++;
    }

    // I had originally set Repeat if we hit anything
    // but this caused a suprising large number of false positives
    // with common words like "of" or "that"
    // to account for this, we'll require a larger number of repeat "hits"
    // before deciding to ignore a line
    //
    // ok, back to any hits
    // need to rethink the strategy here
    if (hits > 0)
      return gTrue;
    else
      return gFalse;
}

GBool AltlawDoc::isRepeatedLine(AltLine *line) {

    // Look for repeated lines - we want to ignore these (headers/footers/etc)
    int hits = 0;
    for(int i=0; i < line->strings()->getLength(); i++) {
      AltString *str = (AltString *)(line->strings()->get(i));

      // bail if we find any non-repeats
      if (this->isRepeatedStr(str))
        hits++;
    }
    // all strings in line are repeats
    if (hits > (0.5 * line->strings()->getLength()) )
      return gTrue;
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

    } else { printf("%c",c); }
  }
}

void AltString::print() {
  //printf("(%.0f,%.0f)",this->X(),this->dX());
  printCString(this->getCString(), this->getLength());
}

void AltWord::print() {
  printf("<WORD coords=\"%.0f,%.0f,%.0f,%.0f\" caps=\"%d\" font=\"%.1f\">",_x1,_y1,_x2,_y2,this->capMode(),this->fontSize());
  if(this->sup) printf("<sup>");
  printCString(this->getCString(), this->getLength());
  if(this->sup) printf("</sup>");
  printf("</WORD>\n");
}

void AltLine::print() {
  printf("<LINE type=\"%d\" caps=\"%d\" avgWordHeight=\"%.1f\" avgCharWidth=\"%.1f\" align=\"%.2f\" vertPos=\"%.2f\" words=\"%d\">\n",
  	this->type(), this->capMode, this->avgWordHeight, this->avgCharWidth, this->align, this->vertPos, this->wordCount);
  // for(int j=0; j < this->strings()->getLength(); j++)
  //  ((AltString*)(this->strings()->get(j)))->print();
  //printf("\n");
  for(int j=0; j < this->words()->getLength(); j++) {
    AltWord *w = (AltWord *)(this->words()->get(j));
    //if (j != 0 and not w->sup) printf(" ");
    w->print();
  }
  printf("</LINE>\n");

/*
  AltString *last = NULL;
  for(int j=0; j < this->strings()->getLength(); j++) {
    AltString *s = (AltString *)(this->strings()->get(j));
    if (j > 0 && fabs(s->X() - (last->X() + last->dX())) > 1.0 )
      printf(" ");
    if (j == 0 and this->looksLikeFootnote()) {
      printf("<sup>");
      s->print();
      printf("</sup>");
    } else { s->print(); }
    last = s;
  }
*/
}

void AltlawDoc::print() {
  AltString *last = NULL;

  // log ignored repeats in a comment
  printf("<!-- these lines looked like repeats and were skipped:\n");
  for(int i=0; i < repeats.getLength(); i++) {
    ((AltLine *)(repeats.get(i)))->print();
    printf("\n");
  }
  printf("-->\n");

  // header
  printf("<div id=\"header\">\n");
  for(int i=0; i < header.getLength(); i++) {
    printf("<p class=\"header\">");
    ((AltLine *)(header.get(i)))->print();
    printf("</p>\n");
  }
  printf("</div>\n\n");

  // body
  printf("<div id=\"body\">\n");
  for(int i=0; i < body.getLength(); i++) {
    AltLine *l = (AltLine *)(body.get(i));

    // markup paragraphs
    switch(l->type()) {
      case(SectionHeading):
        printf("<p class=\"heading\">\n"); // would be nice to include the tab depth
	break;
      case(Body):
        printf("<p class=\"unknown\">");
	break;
      case(PStart):
        printf("<p class=\"body\">\n");
	break;
      case(Paragraph):
        if ((i == 0) or ((((AltLine *)(body.get(i-1)))->type() != Paragraph) and ((AltLine *)(body.get(i-1)))->type() != PStart))
          printf("<p class=\"body-notab\">\n");
	break;

      // only 1 blockquote type, so need to check if this is the first
      case(BlockQuote):
        if ((i == 0) or (((AltLine *)(body.get(i-1)))->type() != BlockQuote))
          printf("<p class=\"blockquote\">\n");
	break;
      default:
        break;
    }
    l->print();
    printf("\n");

    // end paragraphs when necessary
    switch(l->type()) {
      case(Body):
      case(SectionHeading):
      case(PEnd):
        printf("</p>\n");
	break;
      case(Paragraph):
      case(BlockQuote):
        if ((i+1) == body.getLength() or (((AltLine *)(body.get(i+1)))->type() != l->type()))
          printf("</p>\n");
	break;
      default:
        break;
    }
  }
  printf("</div>\n\n");

  // footnotes
  printf("<div id=\"footnotes\">\n");
  for(int i=0; i < footnotes.getLength(); i++) {
    AltLine *l = (AltLine *)(footnotes.get(i));

    // decide when to start a new footnote (all have type = Footnote)
    if (l->looksLikeFootnote()) {
      if (i != 0)
        printf("</p>\n");
      printf("<p id=\"fn%s\">\n",((AltString *)(l->strings()->get(0)))->getCString());
    }
    //printf("<!-- (%.1f,%.1f) -->",l->Y(),l->X());
    l->print();
    /*
    for(int j=0; j < l->strings()->getLength(); j++) {
      AltString *s = (AltString *)(l->strings()->get(j));
      if (j > 0 && fabs(s->X() - (last->X() + last->dX())) > 1.0 )
        printf(" ");
      if(j == 0 and l->looksLikeFootnote())
        printf("<sup>%s</sup> ",s->getCString());
      else
        printf("%s", s->getCString());
      last = s;
    }
    */
    printf("\n");
  }
  // close last footnote (if there were some)
  if (footnotes.getLength() > 0)
      printf("</p>\n");
  printf("</div>\n\n");


  /*
  for(int i=0; i < lines.getLength(); i++) {
    AltLine *l = (AltLine *)(lines.get(i));
    //if (l->type() != Repeat) continue;
    printf("(%d,%.1f,%.1f) [%d (%d,%.2f)]\t",
    	l->page(), l->Y(), l->X(),
	l->type(), l->strings()->getLength(), (l->rightX() / max_x));
    for(int j=0; j < l->strings()->getLength(); j++) {
      AltString *s = (AltString *)(l->strings()->get(j));
      if (j > 0 && fabs(s->X() - (last->X() + last->dX())) > 1.0 )
        printf(" ");
      printf("%s", s->getCString());
      last = s;
    }
    printf("\n");
  }
  //printf("Min X: %.1f\tMax Chars: %d\n", min_x, max_chars);
  */
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
    if (s->yOverlap(y1,y2) <= 0.33 * s->fontSize()) {

      // start a new line
      y1 = s->yMin();
      y2 = s->yMax();
      line = new AltLine(this);
      lines.append(line);
    } else {
      // lets make sure we want to 
    }
    line->add(s);
  }
  for(int i=0; i<lines.getLength(); i++) {
    line = (AltLine *)(lines.get(i));
    line->sort();
    line->parseWords();
    line->calcFeatures();
  }

  // Figure out line spacing / tabs / etc

  // right margin
  GooList *xlist = new GooList();

  for(int i=0; i<lines.getLength(); i++)
    xlist->append(new AltDataPoint(((AltLine *)(lines.get(i)))->rightX()));

  xlist->sort(DataCmp);

  // Let's use the Median (equal number of values higher and lower)
  rightMargin = ((AltDataPoint *)(xlist->get(xlist->getLength() / 2)))->data;
  //printf("len: %d, median: %d\n",xlist->getLength(), xlist->getLength() / 2);
  printf("right margin: %.1f\n",rightMargin);

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
  leftMargin = ((AltDataPoint *)(xlist->get(xlist->getLength() - 1)))->data;
  printf("left margin: %.1f\n",leftMargin);

  /*
  for(int i=0;i<xlist->getLength();i++) {
    AltDataPoint *d = (AltDataPoint *)(xlist->get(i));
    printf("%.1f %d\n", d->data, d->count);
  }
  */

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

  /*
  for(int i=0;i<ylist->getLength();i++) {
    AltDataPoint *d = (AltDataPoint *)(ylist->get(i));
    printf("%.1f %d\n", d->data, d->count);
  }
  */


  // Try to figure out line types
  AltLine *last = line = NULL;
  AltLine *next = NULL;
  GBool inHeader = gTrue;
  for(int i=0; i<lines.getLength(); i++) {

    // last doesn't like to point to ignored lines :)
    if (line and line->type() != Repeat) last = line;

    line = next ? next : (AltLine *)lines.get(i);

    // Look ahead if possible
    next = (i+1 < lines.getLength()) ? (AltLine *)lines.get(i+1) : NULL;

    // Repeated - like Headers / Footers
    if (this->isRepeatedLine(line) ) {
      line->setType(Repeat);
      repeats.append(line);
    }

    // Footnotes
    else if (line->looksLikeFootnote()) {
      line->setType(Footnote);
      footnotes.append(line);
    }

    // Once we find a footnote, pretty much everything below it
    // is also a footnote
    else if ( last and last->type() == Footnote and line->page() == last->page()) {
      line->setType(Footnote);
      footnotes.append(line);
    }

    // Everything else is either Header or Footer
    else if ( inHeader ) {

      // Try to figure out which line the Body starts...
      if (next and this->looksLikeBody(line) and this->looksLikeBody(next) ) {
        inHeader = gFalse;
        line->setType(PStart);
	body.append(line);
      }
      else {
        line->setType(Header);
	header.append(line);
      }
    }
    else {
      if (line->looksLikeSectionHeading())
        line->setType(SectionHeading);
      else if (!this->onLeftMargin(line) and ((next and (line->X() == next->X())) or (last and last->type() == BlockQuote and (line->X() == last->X())) ) )
        line->setType(BlockQuote);
      else if (this->onTabStop(line,1) and this->onRightMargin(line))
        line->setType(PStart);
      else if (this->onLeftMargin(line) and (this->onRightMargin(line) or (last and ((last->type() == Paragraph) or (last->type() == BlockQuote)))))
        line->setType(Paragraph);
    
      body.append(line);
    }
  }

/*
  // Now run through again and break up paragraph lines
  for(int i=0; i<body.getLength(); i++) {
    AltLine *line = (AltLine *)body.get(i);

    // look for blockquotes that are indented
    if (!this->onLeftMargin(line) and !this->onLeftMargin(next)) {}
    else if (this->onTabStop(line,1) and this->onRightMargin(line))
      line->setType(PStart);
    else if (this->onLeftMargin(line) and this->onRightMargin(line))
      line->setType(Paragraph);
    else if (this->onLeftMargin(line) and last->type() == Paragraph) // **implied !onRightMargin
      line->setType(PEnd);
    
  }
*/

}

GBool AltlawDoc::onLeftMargin(AltLine *line) {
  if (fabs(line->X() - leftMargin) < 1.0) return gTrue;
  else return gFalse;
}

GBool AltlawDoc::onRightMargin(AltLine *line) {
  if ((rightMargin - line->rightX()) < (0.02 * (rightMargin - leftMargin))) return gTrue;
  else return gFalse;
}

GBool AltlawDoc::onTabStop(AltLine *line, int tab) {
  double tabstop = 50.0;
  if (fabs(line->X() - leftMargin - (tab * tabstop)) < (0.5 * tabstop)) return gTrue;
  else return gFalse;
}

void AltlawDoc::drawChar(GfxState *state, double x, double y,
	      double dx, double dy,
	      double originX, double originY,
	      CharCode code, int /*nBytes*/, Unicode *u, int uLen) 
{
  printf("drawChar(%f,%f)[%d - %d];\n",x,dx,code,uLen);

}

void AltlawDoc::drawImageMask(GfxState *state, Object *ref, Stream *str,
			      int width, int height, GBool invert,
			      GBool inlineImg) {
printf("AltlawDoc::drawImageMask(...);\n");
}

void AltlawDoc::drawImage(GfxState *state, Object *ref, Stream *str,
			  int width, int height, GfxImageColorMap *colorMap,
			  int *maskColors, GBool inlineImg) {
  printf("AltlawDoc::drawImage(...);\n");
}
