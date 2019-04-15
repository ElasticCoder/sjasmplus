#ifndef SJASMPLUS_LISTING_H
#define SJASMPLUS_LISTING_H

#include <cstdint>
#include <vector>
#include <string>
#include <stack>

#include "asm/common.h"
#include "util.h"


class ListingWriter : public TextOutput {
private:
    Assembler &Asm;
    bool IsActive = false;
    bool OmitLine = false;
    bool InMacro = false;
    std::stack<bool> MacroStack;
    std::vector<uint8_t> ByteBuffer;
    int PreviousAddress;
    aint epadres;
    int NumDigitsInLineNumber = 0;

    void listBytes4();

    void listBytes5();

    void listBytesLong(int pad, const std::string &Prefix);

    std::string printCurrentLocalLine();

public:
    ListingWriter() = delete;

    explicit ListingWriter(Assembler &_Asm) : Asm{_Asm} {}

    void init(fs::path &FileName) override;

    void initPass();

    void listLine(const char *Line);

    void listLineSkip(const char *Line);

    void addByte(uint8_t Byte);

    void setPreviousAddress(int Value) {
        PreviousAddress = Value;
    }

    void omitLine() {
        OmitLine = true;
    }

    void startMacro() {
        MacroStack.push(InMacro);
        InMacro = true;
    }

    void endMacro() {
        if (!MacroStack.empty()) {
            InMacro = MacroStack.top();
            MacroStack.pop();
        } else {
            InMacro = false;
        }
    }
};

#endif //SJASMPLUS_LISTING_H
