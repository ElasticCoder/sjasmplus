//
// Z80 Memory modelling and management
//

#ifndef SJASMPLUS_MEMORY_H
#define SJASMPLUS_MEMORY_H

#include <string>
#include <map>
#include <vector>
#include <array>
#include <bitset>
#include <cstdint>
#include <boost/optional.hpp>
#include "errors.h"

using namespace std::string_literals;

class MemModel {
protected:
    std::string Name;
    bool ZXSysVarsInitialized = false;
public:
    explicit MemModel(const std::string &name) : Name(name) { }

    virtual ~MemModel() = default;

    const std::string &getName() { return Name; }

    virtual bool isPagedMemory() = 0;

    virtual int getNumMemPages() = 0;

    virtual int getDefaultSlot() = 0;

    virtual int getPageNumInSlot(int Slot) = 0;

    virtual uint8_t readByte(uint16_t Addr) = 0;

    virtual void writeByte(uint16_t Addr, uint8_t Byte, bool Ephemeral) = 0;

    void writeWord(uint16_t Addr, uint16_t Word, bool Ephemeral) {
        writeByte(Addr, (uint8_t) (Word & 0xff), Ephemeral);
        writeByte((uint16_t) (Addr + 1), (uint8_t) (Word >> 8), Ephemeral);
    }

    virtual bool usedAddr(uint16_t Addr) = 0;

    virtual void clearEphemerals() = 0;

    void memcpyToMemory(off_t Offset, const uint8_t *Src, uint16_t Size) {
        // Wrap around the destination address while copying
        for (uint16_t i = 0; i < Size; i++) {
            writeByte((uint16_t) (Offset + i), *(Src + i), false);
        }
    }

    void memsetInMemory(off_t Offset, uint8_t Byte, uint16_t Size) {
        // Wrap around the destination address while setting
        for (uint16_t i = 0; i < Size; i++) {
            writeByte((uint16_t) (Offset + i), Byte, false);
        }
    }

    // Return error string on error
    virtual boost::optional<std::string> setPage(int Slot, int Page) = 0;

    virtual boost::optional<std::string> setPage(uint16_t CurrentAddr, int Page) = 0;

    virtual boost::optional<std::string> validateSlot(int Slot) = 0;

    virtual int getPageForAddress(uint16_t CurrentAddr) = 0;

    // TODO: replace with safer version
    virtual void getBytes(uint8_t *Dest, uint16_t Addr, uint16_t Size) = 0;

    virtual void getBytes(uint8_t *Dest, int Slot, uint16_t AddrInPage, uint16_t Size) = 0;

    virtual const uint8_t *getPtrToMem() = 0;

    virtual void clear() = 0;

    virtual const uint8_t *getPtrToPage(int Page) = 0;

    virtual const uint8_t *getPtrToPageInSlot(int Slot) = 0;

    virtual void initZXSysVars() = 0;
};

// Plain 64K without paging
class PlainMemModel : public MemModel {
private:
    std::array<uint8_t, 0x10000> Memory;
    std::bitset<0x10000> MemUsage;
public:
    PlainMemModel() : MemModel{"PLAIN"s} {
        clear();
    }

    ~PlainMemModel() override = default;

    bool isPagedMemory() override { return false; }

    int getNumMemPages() override { return 0; }

    int getDefaultSlot() override { return 0; }

    int getPageNumInSlot(int Slot) override { return 0; }

    boost::optional<std::string> setPage(int Slot, int Page) override {
        return "The PLAIN memory model does not support page switching"s;
    }

    boost::optional<std::string> setPage(uint16_t currentAddr, int Page) override {
        return setPage((int) 0, 0);
    }

    boost::optional<std::string> validateSlot(int Slot) override {
        return setPage((int) 0, (int) 0);
    }

    int getPageForAddress(uint16_t CurrentAddr) override {
        return 0;
    }

    uint8_t readByte(uint16_t Addr) override {
        return Memory[Addr];
    }

    void getBytes(uint8_t *Dest, uint16_t Addr, uint16_t Size) override {
        for (int i = 0; i < Size; i++) {
            *(Dest + i) = Memory[Addr + i];
        }
    }

    void getBytes(uint8_t *Dest, int Slot, uint16_t AddrInPage, uint16_t Size) override {
        Fatal("GetBytes()"s, *(setPage(0, 0)));
    }

    const uint8_t *getPtrToMem() override {
        return Memory.data();
    }

    void clear() override {
        Memory.fill(0);
        MemUsage.reset();
    }

    const uint8_t *getPtrToPage(int Page) override {
        Fatal("GetPtrToPage()"s, *(setPage(0, 0)));
    }

    const uint8_t *getPtrToPageInSlot(int Slot) override {
        Fatal("GetPtrToPageInSlot()"s, *(setPage(0, 0)));
    }

    void writeByte(uint16_t Addr, uint8_t Byte, bool Ephemeral) override {
        Memory[Addr] = Byte;
        if (!Ephemeral)
            MemUsage[Addr] = true;
    }

    bool usedAddr(uint16_t Addr) override {
        return MemUsage[Addr];
    }

    void clearEphemerals() override {
        for (size_t i = 0; i < Memory.size(); i++) {
            if (!MemUsage[i])
                Memory[i] = 0;
        }
    }

    void initZXSysVars() override;
};

// ZX Spectrum 128, 256, 512, 1024 with 4 slots of 16K each
class ZXMemModel : public MemModel {
private:
    const size_t PageSize = 0x4000;
    int NumPages;
    int NumSlots = 4;
    int SlotPages[4] = {0, 5, 2, 7};
    std::vector<uint8_t> Memory;
    std::vector<bool> MemUsage;
    off_t addrToOffset(uint16_t Addr) {
        return SlotPages[Addr / PageSize] * PageSize + (Addr % PageSize);
    }

public:
    ZXMemModel(const std::string &Name, int NPages);

    ~ZXMemModel() override = default;

    uint8_t readByte(uint16_t Addr) override {
        return Memory[addrToOffset(Addr)];
    }

    void getBytes(uint8_t *Dest, uint16_t Addr, uint16_t Size) override {
        for (int i = 0; i < Size; i++) {
            *(Dest + i) = readByte(Addr + i);
        }
    }

    void getBytes(uint8_t *Dest, int Slot, uint16_t AddrInPage, uint16_t Size) override {
        uint16_t addr = AddrInPage + Slot * PageSize;
        for (int i = 0; i < Size; i++) {
            *(Dest + i) = readByte(addr + i);
        }
    }

    const uint8_t *getPtrToMem() override {
        return (const uint8_t *) Memory.data();
    }

    void clear() override {
        Memory.assign(Memory.size(), 0);
        MemUsage.assign(MemUsage.size(), false);
    }

    const uint8_t *getPtrToPage(int Page) override {
        return (const uint8_t *) Memory.data() + Page * PageSize;
    }

    const uint8_t *getPtrToPageInSlot(int Slot) override {
        return (const uint8_t *) Memory.data() + SlotPages[Slot] * PageSize;
    }

    void writeByte(uint16_t Addr, uint8_t Byte, bool Ephemeral) override {
        auto i = addrToOffset(Addr);
        Memory[i] = Byte;
        if (!Ephemeral)
            MemUsage[i] = true;
    }

    bool usedAddr(uint16_t Addr) override {
        return MemUsage[addrToOffset(Addr)];
    }

    void clearEphemerals() override {
        for (size_t i = 0; i < Memory.size(); i++) {
            if (!MemUsage[i])
                Memory[i] = 0;
        }
    }

    void writeByteToPage(int Page, uint16_t Offset, uint8_t Byte) {
        if (Offset >= PageSize)
            Fatal("In-page offset "s + std::to_string(Offset)
                  + " does not fit in page of size "s + std::to_string(PageSize));
        Memory[PageSize * Page + Offset] = Byte;
        MemUsage[PageSize * Page + Offset] = true;
    }

    void memcpyToPage(int Page, off_t Offset, const uint8_t *Src, uint16_t Size) {
        // Wrap around the destination address while copying
        for (uint16_t i = 0; i < Size; i++) {
            writeByteToPage(Page, (uint16_t) (Offset + i), *(Src + i));
        }
    }

    void memsetInPage(int Page, off_t Offset, uint8_t Byte, uint16_t Size) {
        // Wrap around the destination address while setting
        for (uint16_t i = 0; i < Size; i++) {
            writeByteToPage(Page, (uint16_t) (Offset + i), Byte);
        }
    }

    bool isPagedMemory() override { return true; }

    int getNumMemPages() override { return NumPages; }

    int getDefaultSlot() override { return 3; }

    int getPageNumInSlot(int Slot) override { return SlotPages[Slot]; }

    boost::optional<std::string> setPage(int Slot, int Page) override;

    boost::optional<std::string> setPage(uint16_t CurrentAddr, int Page) override;

    boost::optional<std::string> validateSlot(int Slot) override;

    int getPageForAddress(uint16_t CurrentAddr) override;

    void initZXSysVars() override;
};

// MemoryManager knows about memory models and manages them, and is used to collect assembler's output
class MemoryManager {

private:
    std::map<std::string, MemModel *> MemModels;
    MemModel *CurrentMemModel;

    const std::map<std::string, int> MemModelNames = {
            {"PLAIN"s,          0},
            {"ZXSPECTRUM128"s,  8},
            {"ZXSPECTRUM256"s,  16},
            {"ZXSPECTRUM512"s,  32},
            {"ZXSPECTRUM1024"s, 64}
    };

public:
    MemoryManager();

    ~MemoryManager();

    bool isActive() { return CurrentMemModel != nullptr; }

    void setMemModel(const std::string &name);

    MemModel &getMemModel() {
        return *CurrentMemModel;
    }

    const std::string &getMemModelName() {
        return CurrentMemModel->getName();
    }

    bool isPagedMemory() {
        return CurrentMemModel->isPagedMemory();
    }

    int numMemPages() {
        return CurrentMemModel->getNumMemPages();
    }

    int defaultSlot() {
        return CurrentMemModel->getDefaultSlot();
    }

    int getPageNumInSlot(int Slot) {
        return CurrentMemModel->getPageNumInSlot(Slot);
    }

    boost::optional<std::string> setPage(int Slot, int Page) {
        return CurrentMemModel->setPage(Slot, Page);
    }

    boost::optional<std::string> setPage(uint16_t CurrentAddr, int Page) {
        return CurrentMemModel->setPage(CurrentAddr, Page);
    }

    boost::optional<std::string> validateSlot(int Slot) {
        return CurrentMemModel->validateSlot(Slot);
    }

    int getPageForAddress(uint16_t Addr) {
        return CurrentMemModel->getPageForAddress(Addr);
    }

    void getBytes(uint8_t *Dest, uint16_t Addr, uint16_t Size) {
        CurrentMemModel->getBytes(Dest, Addr, Size);
    }

    void getBytes(uint8_t *Dest, int Slot, uint16_t AddrInPage, uint16_t Size) {
        CurrentMemModel->getBytes(Dest, Slot, AddrInPage, Size);
    }

    const uint8_t *getPtrToMem() {
        return CurrentMemModel->getPtrToMem();
    }

    void clear() {
        return CurrentMemModel->clear();
    }

    const uint8_t *getPtrToPage(int Page) {
        return CurrentMemModel->getPtrToPage(Page);
    }

    const uint8_t *getPtrToPageInSlot(int Slot) {
        return CurrentMemModel->getPtrToPageInSlot(Slot);
    }

    void writeByte(uint16_t Addr, uint8_t Byte) {
        CurrentMemModel->writeByte(Addr, Byte, false);
    }

    void initZXSysVars() {
        CurrentMemModel->initZXSysVars();
    }
};


const uint8_t ZXSysVars[] = {
        0x0D, 0x03, 0x20, 0x0D, 0xFF, 0x00, 0x1E, 0xF7,
        0x0D, 0x23, 0x02, 0x00, 0x00, 0x00, 0x16, 0x07,
        0x01, 0x00, 0x06, 0x00, 0x0B, 0x00, 0x01, 0x00,
        0x01, 0x00, 0x06, 0x00, 0x3E, 0x3F, 0x01, 0xFD,
        0xDF, 0x1E, 0x7F, 0x57, 0xE6, 0x07, 0x6F, 0xAA,
        0x0F, 0x0F, 0x0F, 0xCB, 0xE5, 0xC3, 0x99, 0x38,
        0x21, 0x00, 0xC0, 0xE5, 0x18, 0xE6, 0x00, 0x3C,
        0x40, 0x00, 0xFF, 0xCC, 0x01, 0xFC, 0x5F, 0x00,
        0x00, 0x00, 0xFE, 0xFF, 0xFF, 0x01, 0x00, 0x02,
        0x38, 0x00, 0x00, 0xD8, 0x5D, 0x00, 0x00, 0x26,
        0x5D, 0x26, 0x5D, 0x3B, 0x5D, 0xD8, 0x5D, 0x3A,
        0x5D, 0xD9, 0x5D, 0xD9, 0x5D, 0xD7, 0x5D, 0x00,
        0x00, 0xDB, 0x5D, 0xDB, 0x5D, 0xDB, 0x5D, 0x2D,
        0x92, 0x5C, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x4A, 0x17, 0x00, 0x00,
        0xBB, 0x00, 0x00, 0x58, 0xFF, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x21, 0x17, 0x00, 0x40, 0xE0, 0x50,
        0x21, 0x18, 0x21, 0x17, 0x01, 0x38, 0x00, 0x38,
        0x00, 0x00, 0xAF, 0xD3, 0xF7, 0xDB, 0xF7, 0xFE,
        0x1E, 0x28, 0x03, 0xFE, 0x1F, 0xC0, 0xCF, 0x31,
        0x3E, 0x01, 0x32, 0xEF, 0x5C, 0xC9, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xFF, 0x5F, 0xFF, 0xFF, 0xF4, 0x09,
        0xA8, 0x10, 0x4B, 0xF4, 0x09, 0xC4, 0x15, 0x53,
        0x81, 0x0F, 0xC9, 0x15, 0x52, 0x34, 0x5B, 0x2F,
        0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x22,
        0x31, 0x35, 0x36, 0x31, 0x36, 0x22, 0x03, 0xDB,
        0x5C, 0x3D, 0x5D, 0xA2, 0x00, 0x62, 0x6F, 0x6F,
        0x74, 0x20, 0x20, 0x20, 0x20, 0x42, 0x9D, 0x00,
        0x9D, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x08, 0x00, 0x00,
        0x00, 0x00, 0x08, 0xFF, 0xFF, 0xFF, 0x80, 0x00,
        0x00, 0xFF, 0xFA, 0x5C, 0xFA, 0x5C, 0x09, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
        0x00, 0x3C, 0x5D, 0xFC, 0x5F, 0xFF, 0x3C, 0xAA,
        0x00, 0x00, 0x01, 0x02, 0xF8, 0x5F, 0x00, 0x00,
        0xF7, 0x22, 0x62
};

const uint8_t BASin48Vars[] = {
        0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x23, 0x05, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x06, 0x00, 0x0b, 0x00, 0x01, 0x00, 0x01, 0x00,
        0x06, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x3c, 0x40, 0x00, 0xff, 0xc0, 0x01, 0x54, 0xff, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xff, 0xfe, 0xff, 0x01, 0x38, 0x00, 0x00, 0xcb, 0x5c, 0x00,
        0x00, 0xb6, 0x5c, 0xb6, 0x5c, 0xcb, 0x5c, 0xdb, 0x5c, 0xca, 0x5c, 0xcc, 0x5c,
        0xd4, 0x5c, 0xda, 0x5c, 0xcf, 0x00, 0xdc, 0x5c, 0xdc, 0x5c, 0xdc, 0x5c, 0x2d,
        0x92, 0x5c, 0x10, 0x02, 0x00, 0x00, 0xfe, 0xff, 0x01, 0x00, 0x00, 0x00, 0xb6,
        0x1a, 0x00, 0x00, 0xe5, 0x00, 0x00, 0x58, 0xff, 0x00, 0x00, 0x21, 0x00, 0x5b,
        0x21, 0x17, 0x00, 0x40, 0xe0, 0x50, 0x21, 0x18, 0x21, 0x17, 0x01, 0x38, 0x00,
        0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x57, 0xff, 0xff, 0xff,
        0xf4, 0x09, 0xa8, 0x10, 0x4b, 0xf4, 0x09, 0xc4, 0x15, 0x53, 0x81, 0x0f, 0xc4,
        0x15, 0x52, 0xf4, 0x09, 0xc4, 0x15, 0x50, 0x80, 0x80, 0xf9, 0xc0, 0x33, 0x32,
        0x37, 0x36, 0x38, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x80, 0x00, 0x0d, 0x80, 0x00,
        0x00, 0x00, 0x80, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t BASin48SP[] = {
        0xb1, 0x33, 0xe0, 0x5c, 0xc2, 0x02, 0x4d, 0x00, 0xdc, 0x5c, 0x00, 0x80, 0x2b,
        0x2d, 0x54, 0x00, 0x2b, 0x2d, 0x2b, 0x2d, 0x65, 0x33, 0x00, 0x00, 0xed, 0x10,
        0x0d, 0x00, 0x09, 0x00, 0x85, 0x1c, 0x10, 0x1c, 0x52, 0x1b, 0x76, 0x1b, 0x03,
        0x13, 0x00, 0x3e, 0x00, 0x3c, 0x42, 0x42, 0x7e, 0x42, 0x42, 0x00, 0x00, 0x7c,
        0x42, 0x7c, 0x42, 0x42, 0x7c, 0x00, 0x00, 0x3c, 0x42, 0x40, 0x40, 0x42, 0x3c,
        0x00, 0x00, 0x78, 0x44, 0x42, 0x42, 0x44, 0x78, 0x00, 0x00, 0x7e, 0x40, 0x7c,
        0x40, 0x40, 0x7e, 0x00, 0x00, 0x7e, 0x40, 0x7c, 0x40, 0x40, 0x40, 0x00, 0x00,
        0x3c, 0x42, 0x40, 0x4e, 0x42, 0x3c, 0x00, 0x00, 0x42, 0x42, 0x7e, 0x42, 0x42,
        0x42, 0x00, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x3e, 0x00, 0x00, 0x02, 0x02,
        0x02, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x44, 0x48, 0x70, 0x48, 0x44, 0x42, 0x00,
        0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7e, 0x00, 0x00, 0x42, 0x66, 0x5a, 0x42,
        0x42, 0x42, 0x00, 0x00, 0x42, 0x62, 0x52, 0x4a, 0x46, 0x42, 0x00, 0x00, 0x3c,
        0x42, 0x42, 0x42, 0x42, 0x3c, 0x00, 0x00, 0x7c, 0x42, 0x42, 0x7c, 0x40, 0x40,
        0x00, 0x00, 0x3c, 0x42, 0x42, 0x52, 0x4a, 0x3c, 0x00, 0x00, 0x7c, 0x42, 0x42,
        0x7c, 0x44, 0x42, 0x00, 0x00, 0x3c, 0x40, 0x3c, 0x02, 0x42, 0x3c, 0x00, 0x00,
        0xfe, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x42,
        0x3c, 0x00, 0x00
};

#endif //SJASMPLUS_MEMORY_H
