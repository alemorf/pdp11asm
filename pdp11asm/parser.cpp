//! Добавить H числа
// PDP11 Assembler (c) 15-01-2015 vinxru

#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <memory>

//-----------------------------------------------------------------------------

template<class T> inline T& get(std::list<T>& a, int n) {
    typename std::list<T>::iterator i = a.begin();
    while(n--)
        i++;
    return *i;
}

//-----------------------------------------------------------------------------

static int findx(const char** a, const char* s, size_t si) {
  for(int i=0; *a; a++, i++)
    if(0==strncmp(*a, s, si))
      return i;
  for(int i=0; *a; a++, i++)
    if(0==strcmp(*a, s))
      return i;
  return -1;
}

//-----------------------------------------------------------------------------

Parser::Parser() {
  macroOff=false;

  cfg.caseSel=false;
  cfg.cescape=false;
  cfg.remark = 0;
  cfg.bremark = 0;
  cfg.eremark = 0;
  cfg.operators = 0;
  cfg.decimalnumbers = false;
  cfg.prep = 0;

  sigCursor = prevCursor = cursor = 0;
  sigLine = prevLine = line = 1;
  sigCol = prevCol = col = 1;

  token = ttEof;
  tokenNum = 0;
  tokenTextSize = 0;
  tokenText[0] = 0;

  loadedNum = 0;
  loadedText[0] = 0;
  loadedTextSize = 0;

  linkFrom = 01000;

  init("");
}

//-----------------------------------------------------------------------------

void Parser::getLabel(Label& label) {
  label.col = prevCol;
  label.line = prevLine;
  label.cursor = prevCursor;
}

//-----------------------------------------------------------------------------

void Parser::init(const char* buf) {
  line = col = 1;
  firstCursor = prevCursor = cursor = buf;
  token = ttEof;   
  cfg.altstringb = 0;
  nextToken();
}

//-----------------------------------------------------------------------------

void Parser::nextToken() {
  // Сохраняем прошлый токен
  switch((unsigned)token) {
    case ttWord:
    case ttString1:
    case ttString2:
      strcpy(loadedText, tokenText);
      loadedTextSize = tokenTextSize;
      break;
    case ttInteger:
      loadedNum = tokenNum;
  }

  // sig - это до пробелов
  sigCol=col-1;
  sigLine=line;
  sigCursor = cursor;

    do {
        for(;*cursor==' ' || *cursor=='\r' || (!cfg.eol && *cursor=='\n') || *cursor=='\t'; cursor++)
            if(*cursor=='\n') { line++; col=1; }
            else if(*cursor!='\r') col++;

        // Парсим
        tokenText[0]=0;
retry:
        // Пропускаем пробелы
        prevLine=line;
        prevCol =col;
        prevCursor=cursor;

        for(;;) {
            char c=*cursor;
            if(c==0) {
            if(macroStack.size()>0) { leaveMacro(); goto retry; }
            token=ttEof;
            return;
        }
        if(c!=' ' && c!='\n' && c!='\r' && c!='\t') break;
        cursor++;
        if(c=='\n') line++, col=1;
        if(cfg.eol && c=='\n') {
            token=ttEol;
            return;
        }
    }

    //
    const char* s=cursor;
    nextToken2();

    // Увеличиваем курсор
    for(;s<cursor;s++)
      if(*s==10) { line++; col=1; } else
      if(*s!=13) col++;

    while(!macroOff && cfg.prep && token==ttOperator && tokenText[0]=='#')
    {
      Parser::ParserMacroOff md(*this);

      prepCfg = cfg;
      cfg.altstringb = '<';
      cfg.altstringe = '>';
      cfg.eol = true;
      macroOff = true;

      needToken("#");
      //TokenText cmd;
      //strcpy(cmd, needIdent());
      //std::string body;
      //readDirective(body);
      cfg.prep(*this);
      nextToken(); //! Убрать рекурсию
    }
  } while(token==ttComment);

  if(token==ttWord  && !macroOff) {
    int i=0;
    for(std::list<Macro>::iterator m = macro.begin(); m != macro.end(); m++, i++) {
      if(!m->disabled && 0==strcmp(m->id.c_str(), tokenText)) {
        nextToken();
        if(m->args.size()>0) {
          if(token!=ttOperator || 0!=strcmp(tokenText, "(")) syntaxError("Expected operator or (");
          std::vector<std::string> noArgs;
          for(unsigned int j=0; j<m->args.size(); j++) {
            std::string out;
            readComment(out, j+1 < m->args.size() ? "," : ")", false);
            addMacro(m->args[j].c_str(), out.c_str(), noArgs);
          }
          prevCursor = cursor; prevLine = line; prevCol = col;
        }
        m->disabled = true;
        enterMacro(m->args.size(), 0, i, false);
        init(m->body.c_str());
        return;
      }
    }
  }
}

//-----------------------------------------------------------------------------

void Parser::exitPrep()
{
    cfg = prepCfg;
    macroOff = false;
    if(token!=ttEof && token!=ttEol) syntaxError("Expected EOF or EOL");
}

//-----------------------------------------------------------------------------

bool Parser::ifToken(const char* t) {
  if(token==ttWord || token==ttOperator) { 
    if(cfg.caseSel) {
      if(0 == strcmp(tokenText, t)) { nextToken(); return true; }
    } else {
      if(0 == strcasecmp(tokenText, t)) { nextToken(); return true; }
    }    
  }
  return false;
}

//-----------------------------------------------------------------------------

void Parser::nextToken2() {
  size_t tokenText_ptr = 0;
  tokenText[0] = 0; //! important

  char c=*cursor++;

  if(c=='_' || (c>='a' && c<='z') || (c>='A' && c<='Z')) {
    for(;;) {
      if(tokenText_ptr == maxTokenText) syntaxError("Too large word");
      tokenText[tokenText_ptr++]=c;
      c = *cursor;
      if(!(c=='_' || (c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z'))) break;
      cursor++;
    }
    tokenText[tokenText_ptr]=0;
    token=ttWord;

    if(!cfg.caseSel)
      for(char* p = tokenText, *pe = tokenText + tokenText_ptr; p < pe; p++)
        *p = (char)toupper(*p);
    return;
  }

  if(c=='\'' || c=='"' || c==cfg.altstringb) {
    char quoter = c==cfg.altstringb ? cfg.altstringe : c;
    for(;;) {
      c=*cursor;
      if(c==0 || c==10 || c==13) syntaxError("Unterminated string");
      cursor++; 
      if(!cfg.cescape) {
        if(c==quoter) {
          if(*cursor!=quoter) break;
          cursor++; 
        } else 
        if(c==quoter) break;
      } else {
        if(c=='\\') { 
          c=*cursor++;
          if(c==0 || c==10) syntaxError("Unterminated string"); else
          if(c=='n') c='\n'; else
          if(c=='r') c='\r'; else
          if(c=='\\') c='\\'; else
          if(c=='\'') c='\''; else
          if(c=='"') c='"'; else
          if(c=='x') {
            char c1=*cursor++;
            if(c1==0 || c1==10) syntaxError("Unterminated string");
            if(c1>='0' && c1<='9') c1-='0'; else
            if(c1>='a' && c1<='f') c1-='a'-10; else
            if(c1>='A' && c1<='F') c1-='A'-10; else
              syntaxError("Unknown esc");
            char c2=*cursor++;
            if(c2==0 || c2==10) syntaxError("Unterminated string");
            if(c2>='0' && c2<='9') c2-='0'; else
            if(c2>='a' && c2<='f') c2-='a'-10; else
            if(c2>='A' && c2<='F') c2-='A'-10; else
              syntaxError("Unknown esc");
            c=(c1<<4)+c2;
          } else {
            syntaxError("Unknown esc");
          }
        } else 
        if(c==quoter) break;
      }
      if(tokenText_ptr==maxTokenText) syntaxError("Too large string");
      tokenText[tokenText_ptr++]=c;
    }        
    tokenText[tokenText_ptr]=0;
    tokenTextSize = tokenText_ptr;
    token=quoter=='\'' ? ttString1 : ttString2;
    return;
  }

  bool neg = false;
  if(c=='-') {
    neg = true;
    if(*cursor>='0' && *cursor<='9') {
      c = *cursor++;
    }
  }
  if(c>='0' && c<='9') {
    int radix = 0;

    // Если число начинается с 0x - то читаем 16-ричное    
    if(c=='0' && radix==0) {
      if(cursor[0]=='x' || cursor[0]=='X') {
        cursor+=2; // Пропускаем X
        radix = 16;
      } else {
        // Если число начинается с 0 - то читаем 8-ричное    
        radix = 8;
      }
    }

    // Ищем конец числа и определяем возможныцй тип
    bool cb=true, co=true, cd=true;
    cursor--;
    const char* e = cursor; 
    for(;;) {
      c = *e;
      if(!((c>='0' && c<='9') || (c>='A' && c<='F') || (c>='a' && c<='f'))) break;
      e++;
      if(c=='0' || c=='1') continue; cb = false;
      if(c>='0' && c<='7') continue; co = false;
      if(c>='0' && c<='9') continue; cd = false;
    }

    // Постфикс определяет тип числа    
    const char* pe = e;    
    switch(c) {
      case 'b': case 'B': if(radix==16) syntaxError("Incorrect number"); radix = 2; pe++; break;
      case 'o': case 'O': if(radix==16) syntaxError("Incorrect number"); radix = 8; pe++; break;
      case 'h': case 'H': if(radix==16) syntaxError("Incorrect number"); radix = 16; pe++; break;
      case '.':           if(radix==16) syntaxError("Incorrect number"); radix = 10; pe++; break;
      default: if(radix==0) radix = cfg.decimalnumbers ? 10 : 8;
    }

    // Контроль
    switch(radix) {
      case 2: if(!cb) syntaxError("Incorrect number");
      case 8: if(!co) syntaxError("Incorrect number");
      case 10: if(!cd) syntaxError("Incorrect number");
    }

    // Преобразуем
    num_t n = 0;
    for(;cursor < e; cursor++) {
      c = *cursor;      
      n *= radix;
      if(c>='a' && c<='f') n+=(c-'a'+10); else
      if(c>='A' && c<='F') n+=(c-'A'+10); else n+=(c-'0');
    }
    cursor = pe;
    token = ttInteger;
    if(neg) n = 0-n;
    tokenNum = n; 
    return;
  }
 
  // Одиночный символ
  token = ttOperator;
  tokenText[0] = c;
  tokenText[1] = 0;

  // Составной оператор
  if(cfg.operators) {
    // Добавляем остальные символы
    tokenText_ptr = 1;
    for(;;) {
      c = *cursor;
      if(c==0) break;
      tokenText[tokenText_ptr] = c;
      tokenText[tokenText_ptr+1] = 0;
      if(findx(cfg.operators, tokenText, tokenText_ptr+1)==-1) break;
      cursor++;
      tokenText_ptr++;
      if(tokenText_ptr==maxTokenText) syntaxError("Too large operator");
    }
    tokenText[tokenText_ptr]=0;
  }
  // Комментарии
  if(cfg.remark) {
    for(int j=0; cfg.remark[j]; j++) {
      if(0==strcmp(tokenText,cfg.remark[j])) {
        for(;;) {
          c=*cursor;
          if(c==0 || c==10) break;
          cursor++;
        }
        token=ttComment;
        return;
      }
    }
  }
  // Комментарии
  if(cfg.bremark) {
    for(int j=0; cfg.bremark[j]; j++)
      if(0==strcasecmp(tokenText, cfg.bremark[j])) {
        while(true) {
          char c1=c;
          c=*cursor;
          if(c==0) break;
         // if(c==10) line++;
          cursor++;
          if(cfg.eremark[j][1]==0)
            { if(c==cfg.eremark[j][0]) break; }
          else
            { if(c1==cfg.eremark[j][0] && c==cfg.eremark[j][1]) break; }
        }
        token=ttComment; //! c==0
        return;
      }
  }
}

//-----------------------------------------------------------------------------

void Parser::syntaxError(const char* text)
{
    bool i = (token==ttWord || token==ttString1 || token==ttString2 || token==ttOperator);
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s(%zu,%zu): %s%s%s%s", fileName.c_str(), prevLine, prevCol,
       text ? text : "Синтаксическая ошибка", i ? " (" : "", i ? tokenText : "", i ? ")" : "");
    throw std::runtime_error(buf);
}

//-----------------------------------------------------------------------------

void Parser::jump(Label& label) {
  sigCursor = cursor = prevCursor = label.cursor;
  sigLine = prevLine = line = label.line;
  sigCol = prevCol = col = label.col;
  nextToken();
}

//-----------------------------------------------------------------------------

bool Parser::ifToken(const char** a, unsigned& n) {
  for(const char** i = a; *i; i++) {
    if(ifToken(*i)) {
      n = i - a;
      return true;
    }
  }
  return false;
}

//-----------------------------------------------------------------------------

void Parser::enterMacro(int killMacro, std::string* buf, int disabledMacro, bool dontCallNextToken) {
  macroStack.push_back(MacroStack());
  MacroStack& m = macroStack.back();
  getLabel(m.pl);
  m.disabledMacro = disabledMacro;
  //m.cursor    =cursor;
  //m.prevCursor=prevCursor;
  //m.line      =line;
  //m.col       =col;
  //m.prevCol   =prevCol;
  //m.prevLine  =prevLine;
  //m.sigCol    =sigCol;
  //m.sigLine   =sigLine;
  m.killMacro =killMacro;
  m.fileName  = fileName;
  m.prevFirstCursor = firstCursor;
  fileName = "DEFINE";
  if(buf) {
    m.buffer.swap(*buf);

    //loadFromString_noBuf(m.buffer.c_str(), dontCallNextToken);

      line=1;
      col=1;
      firstCursor = prevCursor = cursor = m.buffer.c_str();
      token = ttEof;
      if(!dontCallNextToken) nextToken();
  }
};

//-----------------------------------------------------------------------------

void Parser::leaveMacro() {
  MacroStack& m = macroStack.back(); //get(macroStack, macroStack.size()-1);
  //cursor    =m.cursor;
  //prevCursor=m.prevCursor;
  //line      =m.line;
  //col       =m.col;
  //prevCol   =m.prevCol;
  //prevLine  =m.prevLine;
  fileName    =m.fileName;
  //sigCol    =m.sigCol;
  //sigLine   =m.sigLine;
  firstCursor = m.prevFirstCursor;
  jump(m.pl, true);
  for(int i=0; i<m.killMacro; i++)
    macro.pop_back();
  if(m.disabledMacro!=-1) {
    //assert(macro[m.disabledMacro].disabled);
    get(macro, m.disabledMacro).disabled = false;
  }
  macroStack.pop_back();
}

void Parser::jump(Label& label, bool dontLoadNextToken) {
  sigCursor = cursor = prevCursor = label.cursor;
  sigLine = prevLine = line = label.line;
  sigCol = prevCol = col = label.col;
  if(!dontLoadNextToken) nextToken();
}

void Parser::readDirective(std::string& out)
{
    char c=0, c1;
    const char* start = prevCursor;
    line = prevLine;
    col = prevCol;
    for(;;)
    {
        c1 = c;
        c = *prevCursor;
        if(c == 0)
        {
            out.append(start, prevCursor-start);
            break;
        }
        prevCursor++;
        if(c==10)
        {
            unsigned l = prevCursor-start-1;
            if(c1=='\\')
            {
                line++;
                col=1;
                out.append(start, l-1); //! Проверить, не попадает ли перевод строки
                start = prevCursor;
                continue;
            }
            if(c1==13) l--;
            out.append(start, l); //! Проверить, не попадает ли перевод строки
            break;
        }
        col++;
     }
     cursor = prevCursor;
     token = ttEol;
     //nextToken();
}

void Parser::addMacro(const char* id, const char* body, const std::vector<std::string>& args)
{
    macro.push_back(Macro());
    Macro& m = macro.back();
    m.id   = id;
    m.body = body;
    m.args = args;
}

bool Parser::deleteMacro(const char* id)
{
    for(std::list<Macro>::iterator m = macro.begin(); m != macro.end(); m++)
    {
        if(m->id == id)
        {
            macro.erase(m);
            return true;
        }
    }
    return false;
}

bool Parser::findMacro(const char* id)
{
    for(std::list<Macro>::iterator m = macro.begin(); m != macro.end(); m++)
    {
        if(m->id == id) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------------------------

void Parser::readComment(std::string& out, const char* term, bool cppEolDisabler) {
  const char* t=cursor;
  waitComment(term, cppEolDisabler ? '\\' : 0);
  out.assign(t, cursor-t);
//  if(cppEolDisabler)
//    out = replace("\\\r\n", "\r\n", out);
  int termLen=strlen(term);
  if(0==strcmp(out.c_str()+out.size()-termLen, term)) out.resize(out.size()-termLen);
}

bool Parser::waitComment(const char* erem, char combineLine) {
  const char* s=cursor;
  char c=0;
  bool eof;
  while(true) {
    char c1=c;
    c=*cursor;
    if(c==0) { eof=true; break; } //syntaxError((string)T"Íå íàéäåí êîíåö êîììåíòàðèÿ "+erem[j]);
  //  if(c==10) line++;
    cursor++;
    if(c==combineLine && cursor[0]=='\r') {
      cursor++;
      if(cursor[0]=='\n') cursor++;
      continue;
    }
    if(erem[1]==0) {
      if(c==erem[0]) { eof=false; break; }
    } else {
      if(c1==erem[0] && c==erem[1]) { eof=false; break; }
    }
  }
  // Óâåëè÷èâàåì êóðñîð
  for(;s<cursor;s++)
    if(*s==10) { line++; col=1; } else
    if(*s!=13) col++;
  //
  return eof;
}
