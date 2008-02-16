//=====================
//
// altlaw_parse_pdf.cc
//
//=====================

#include "config.h"
#include <poppler-config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include "parseargs.h"
#include "goo/GooString.h"
#include "goo/gmem.h"
#include "Object.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "PDFDoc.h"
#include "Altlaw.h"
#include "PSOutputDev.h"
#include "GlobalParams.h"
#include "Error.h"
#include "goo/gfile.h"

int main(int argc, char *argv[]) {
  PDFDoc *doc = NULL;
  GooString *fileName = NULL;
  AltlawDoc *AL = NULL;

  GBool ok;

  // read config file
  globalParams = new GlobalParams();
  globalParams->setPrintCommands(gFalse);  // set this to see the opcode guts of the pdf...

  fileName = new GooString(argv[1]);

  doc = new PDFDoc(fileName, NULL, NULL); //ownerPW, userPW);

  if (!doc->isOk()) {
    goto error;
  }

  // Start the Altlaw parser
  AL = new AltlawDoc(doc);

  // print it out
  AL->print(gFalse);

  delete AL;

 
 // clean up
 error:
  if(doc) delete doc;
  if(globalParams) delete globalParams;

  return 0;
}

