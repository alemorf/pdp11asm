// PDP11 Assembler (c) 15-01-2015 vinxru

#pragma once
#include "lstwriter.h"
#include "parser.h"
#include "output.h"
#include <limits>
#include <stdint.h>
#include <stdio.h>
#include "fstools.h"
#include "tools.h"

class Compiler {
public:
    enum Processor { P_PDP11, P_8080 };

    LstWriter lstWriter;
    Parser p;
    Output out;
    bool needCreateOutputFile;
    bool step2;
    bool convert1251toKOI8R;
    Processor processor;
    std::map<std::string, Parser::num_t> labels;
    char lastLabel[Parser::maxTokenText];
    char sourceFile[1024];

    // c_common.cpp
    Compiler();
    void compileFile(const syschar_t* fileName);
    void processLabels();
    bool compileLine2();
    void compileLine();
    bool ifConst3(Parser::num_t& out, bool numIsLabel=false);
    bool ifConst4(Parser::num_t& out, bool numIsLabel=false);
    void makeLocalLabelName();
    void compileByte();
    void compileWord();
    Parser::num_t readConst3(bool numIsLabel=false);
    void compileOrg();
    void disassembly(unsigned s, unsigned e);

    // c_bitmap.cpp
    bool compileLine_bitmap();

    // c_pdp11.cpp
    struct Arg {
        bool used;
        int  val;
        int  code;
        bool subip;
    };

    void write(int n, Arg& a);
    void write(int n, Arg& a, Arg& b);
    bool regInParser();
    int readReg();
    void readArg(Arg& a);
    bool compileLine_pdp11();

    // c_8080.cpp
    bool compileLine_8080();
    unsigned disassembly8080(char* out, uint8_t* x, unsigned l, unsigned pos);

    enum FixupType { ftWord, ftByte, ftByteHigh };

    class Fixup {
    public:
        FixupType type;
        size_t addr;
        std::string name;

        Fixup(FixupType& _type, size_t _addr=0, const std::string& _name="") : type(_type), addr(_addr), name(_name) {}
    };

    std::vector<Fixup> fixups;

    inline void addFixup(FixupType type, const std::string& name, unsigned d=0)
    {
        if(name.empty()) return;
        fixups.push_back(Fixup(type, out.writePtr-d, ucase(name)));
    }

    void addLabel(const std::string& name)
    {
        labels[ucase(name)] = out.writePtr;
    }
};

//-----------------------------------------------------------------------------

inline size_t ullong2size_t(unsigned long long a) {
  if(a > std::numeric_limits<size_t>::max()) throw std::runtime_error("Too big number");
  return (size_t)a;
}

//-----------------------------------------------------------------------------

static const size_t disassemblyPdp11OutSize = 256;

unsigned disassemblyPdp11(char* out, uint16_t* c, unsigned l, unsigned pos);
