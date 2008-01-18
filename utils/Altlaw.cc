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
  min_x = 1100.0;
  max_x = 0.0;
  max_chars = 0;

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
			double x, double y,
			double dx, double dy,
			double fontSize,
			GBool bold, GBool italics) : GooString(s) {
  _page = page;
  _x = x;
  _y = y;
  _dx = dx;
  _dy = dy;
  _fontSize = fontSize;
  _bold = bold;
  _italics = italics;
}

AltLine::AltLine() {
  s = new GooList();
  _x = _y = _dx = _dy = _fontSize = 0.0;
  _page = -1;
  _type = null;
  _chars = 0;
}

// Add another string to the end of the line
void AltLine::add(AltString *str) {
  // add it to the list
  s->append(str);

  // update our line variables
  _chars += str->getLength();
  _dx += str->dX(); // this isn't going to be completely accurate with implied spaces
  _dy += str->dY();

  // use the largest fontSize / Y val of all strings (perhaps the mode/median/mean would be better?)
  _y = str->Y() > _y ? str->Y() : _y;
  _fontSize = str->fontSize() > _fontSize ? str->fontSize() : _fontSize;

  // set line vals with first string
  if (s->getLength() == 1) {
    _page = str->page();
    _x = str->X();
    _y = str->Y();
    _fontSize = str->fontSize();
  }

}

GBool AltlawDoc::looksLikeBody(AltLine *line) {

  if (this->onLeftMargin(line) and this->onRightMargin(line)) return gTrue;
  if (this->onTabStop(line,1) and this->onRightMargin(line)) return gTrue;

  // just in case tab is not = 50.0, which is hardcoded in onTabStop right now
  if (line->X() - min_x < 100.0 and this->onRightMargin(line)) return gTrue;
  return gFalse;

 }

GBool AltLine::isBold() {
  for(int i=0; i<s->getLength(); i++) {
    if(!((AltString *)(s->get(i)))->bold()) {
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

// Should this be in AltlawDoc:: ?
GBool AltLine::looksLikeFootnote() {

  int n = 0;
  AltString *str = (AltString *)(s->get(0));
  char c = str->getChar(0);
  if (c >= '0' and c <= '9') n++;
  if (c == '*') n++;
  if (str->Y() < _y) n++;
  if (str->fontSize() < _fontSize) n++;
  if (str->bold()) n++;
  if (str->italics()) n++;
  if (n >= 3) return gTrue;
  else return gFalse;

}

void AltlawDoc::drawString(GfxState *state, GooString *s) {
  GfxFont *font;
  CharCode code;
  Unicode u[8];
  double x, y, tx, ty, dx, dy, dx2, dy2, tdx, tdy, originX, originY;
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
  // calc raw (dx,dy), chars, and spaces
  while (len > 0) {
    n = font->getNextChar(p, len, &code,
			    u, (int)(sizeof(u) / sizeof(Unicode)), &uLen,
			    &dx2, &dy2, &originX, &originY);
    // do a quick check for non ascii chars
    if (code > 0x7F) {
      int pos = s->getLength() - len;
      switch(code) {
        case(0x91):
        case(0x92):
	  s->setChar(pos, '\'');
	  break;
	case(0x93):
	case(0x94):
	case(0xAA):
	case(0xBA):
	  s->setChar(pos, '"');
	  break;
	case(0x96):
	case(0x97):
	case(0xD0):
	  s->setChar(pos, '-');
	  break;
	case(0xA7):
	  s->del(pos);
	  s->insert(pos, "&sect;");
	  break;
	case(0xB6):
	  s->del(pos);
	  s->insert(pos, "&para;");
	  break;
        case(0xBD):
	  s->setChar(pos, '+');
	  break;
	case(0xFC): // these are weird brackets that sometimes get used on docket sheets
	case(0xFD):
	case(0xFE):
	  s->del(pos);
	  break;
      }
      p = s->getCString() + s->getLength() - len;
    }

    dx += dx2;
    dy += dy2;
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
  dy *= state->getFontSize();

  // apply Transform Matrix
  state->textTransformDelta(dx, dy, &tdx, &tdy);
  state->transform(state->getCurX(),state->getCurY(),&tx,&ty);
  state->transformDelta(tdx,tdy,&dx,&dy);
  strings.append(new AltString(s,pageNum,tx,ty,dx,dy,
  				state->getFontSize(),
				bold,italic));
  //printf("(%.1f,%.1f)+(%.1f,%.1f)\t'%s'\n",ty,tx,dy,dx,s->getCString());
}

// Figure out where the string is in relation to the page/y/x coords given
int AltString::cmpPYX(int page, double y, double x,
			double fudgeY, double fudgeX ) {

  return
    _page > page ?  1 :
    _page < page ? -1 :
    _y > ( y + fudgeY ) ?  1 :
    _y < ( y - fudgeY ) ? -1 :
    _x > ( x + fudgeX ) ?  1 :
    _x < ( x - fudgeX ) ? -1 :
     0;
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
    if (str->cmpPYX(page,y,x,fudgeY,fudgeX) <= 0 )
      p1 = guess + 1;
    else
      //can't be high = mid-1: here A[mid] >= value,
      //so high can't be < mid if A[mid] == value
      p2 = guess; 
  }

  if (p1 < strings.getLength() and str->cmpPYX(page,y,x,fudgeY,fudgeX) == 0 ) {
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
void AltlawDoc::print() {
  AltString *last;

  // log ignored repeats in a comment
  printf("<!-- these lines looked like repeats and were skipped:\n");
  for(int i=0; i < repeats.getLength(); i++) {
    AltLine *l = (AltLine *)(repeats.get(i));
    for(int j=0; j < l->strings()->getLength(); j++) {
      AltString *s = (AltString *)(l->strings()->get(j));
      if (j > 0 && fabs(s->X() - (last->X() + last->dX())) > 1.0 )
        printf(" ");
      printf("%s", s->getCString());
      last = s;
    }
    printf("\n");
  }
  printf("-->\n");

  // header
  printf("<div id=\"header\">\n");
  for(int i=0; i < header.getLength(); i++) {
    AltLine *l = (AltLine *)(header.get(i));
    printf("<p class=\"header\">");
    for(int j=0; j < l->strings()->getLength(); j++) {
      AltString *s = (AltString *)(l->strings()->get(j));
      if (j > 0 && fabs(s->X() - (last->X() + last->dX())) > 1.0 )
        printf(" ");
      printf("%s", s->getCString());
      last = s;
    }
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
    }
    for(int j=0; j < l->strings()->getLength(); j++) {
      AltString *s = (AltString *)(l->strings()->get(j));
      if (j > 0 && fabs(s->X() - (last->X() + last->dX())) > 1.0 )
        printf(" ");
      printf("%s", s->getCString());
      last = s;
    }
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

void AltlawDoc::parse() {

  // Sort strings by Page, Y, X
  this->sort();

  // Group strings into Lines
  double y = -99; // this is the y of the last line
  AltLine *line = NULL;
  AltString *s = NULL;
  for(int i=0; i<strings.getLength(); i++) {
    s = (AltString *)(strings.get(i));

    // need to modify to deal with close but not equal
    if (fabs(s->Y() - y) > ALTFUDGEY) {

      // process completed line
      // calc min_x and max_chars as we go - avoid full scan later...
      if (line) {
        if (line->X() < min_x) min_x = line->X();
        if (line->rightX() > max_x) max_x = line->rightX();
        if (line->chars() > max_chars) max_chars = line->chars();
      }

      // start a new line
      y = s->Y();
      line = new AltLine();
      lines.append(line);
    }
    line->add(s);
  }


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
  if (fabs(line->X() - min_x) < 1.0) return gTrue;
  else return gFalse;
}

GBool AltlawDoc::onRightMargin(AltLine *line) {
  if ((line->rightX() / max_x) >= 0.80) return gTrue;
  else return gFalse;
}

GBool AltlawDoc::onTabStop(AltLine *line, int tab) {
  double tabstop = 50.0;
  if (fabs(line->X() - min_x - (tab * tabstop)) < (0.5 * tabstop)) return gTrue;
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
