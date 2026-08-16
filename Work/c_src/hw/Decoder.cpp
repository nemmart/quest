#include "Decoder.hpp"
#include "NovaCompute.hpp"
#include "EagleCompute.hpp"
#include "EagleGeneral.hpp"
#include "EagleStack.hpp"
#include "EagleFloat.hpp"
#include "EagleSpecial.hpp"
#include "LEFInstruction.hpp"
#include <stdexcept>
#include <cstdio>




namespace hw {
Instruction* Decoder::nova_general[12] = {};
Instruction* Decoder::nova_io[32] = {};
Instruction* Decoder::nova_compute[8] = {};
Instruction* Decoder::eclipse_mv[4096] = {};
LEFInstruction Decoder::lef_instruction;

std::vector<Definition> Decoder::nova_general_opcodes = {
  {"00000piixxxxxxxx", "JMP", "", "", -1},
  {"00001piixxxxxxxx", "JSR", "", "", -1},
  {"00010piixxxxxxxx", "ISZ", "", "", -1},
  {"00011piixxxxxxxx", "DSZ", "", "", -1},
  {"001yypiixxxxxxxx", "LDA", "", "", -1},
  {"010yyaiixxxxxxxx", "STA", "", "", -1}
};

std::vector<Definition> Decoder::nova_io_opcodes = {
  {"01100000ffdddddd", "NIO", "", "", -1},
  {"0110100000111111", "NCLID", "", "", -1},
  {"011yy001ffdddddd", "DIA", "", "", -1},
  {"011yy011ffdddddd", "DIB", "", "", -1},
  {"011yy101ffdddddd", "DIC", "", "", -1},
  {"011yy010ffdddddd", "DOA", "", "", -1},
  {"011yy100ffdddddd", "DOB", "", "", -1},
  {"011yy110ffdddddd", "DOC", "", "", -1},
  {"011yy101ff111111", "IORST", "", "", -1}
};

std::vector<Definition> Decoder::nova_lef_opcodes = {
  {"011yyaiixxxxxxxx", "LEF*", "", "", -1}
};

std::vector<Definition> Decoder::nova_compute_opcodes = {
  {"1xxyy000ssccnkkk", "COM", "NovaCompute", "novaCompute", NovaCompute::COM},
  {"1xxyy001ssccnkkk", "NEG", "NovaCompute", "novaCompute", NovaCompute::NEG},
  {"1xxyy010ssccnkkk", "MOV", "NovaCompute", "novaCompute", NovaCompute::MOV},
  {"1xxyy011ssccnkkk", "INC", "NovaCompute", "novaCompute", NovaCompute::INC},
  {"1xxyy100ssccnkkk", "ADC", "NovaCompute", "novaCompute", NovaCompute::ADC},
  {"1xxyy101ssccnkkk", "SUB", "NovaCompute", "novaCompute", NovaCompute::SUB},
  {"1xxyy110ssccnkkk", "ADD", "NovaCompute", "novaCompute", NovaCompute::ADD},
  {"1xxyy111ssccnkkk", "AND", "NovaCompute", "novaCompute", NovaCompute::AND}
};

std::vector<Definition> Decoder::eclipse_mv_opcodes = {
  {"111yy11111111000", "ADDI", "EagleCompute", "registerWordImmediate", EagleCompute::ADDI},
  {"1nnyy00000001000", "ADI", "", "", -1},
  {"1xxyy00110001000", "ANC", "", "", -1},
  {"110yy11111111000", "ANDI", "EagleCompute", "registerWordImmediate", EagleCompute::ANDI},
  {"1001011111001000", "BAM", "", "", -1},
  {"1100011110001001", "BKPT*", "", "", -1},
  {"1011011111001000", "BLM", "", "", -1},
  {"1xxyy10000001000", "BTO", "", "", -1},
  {"1xxyy10001001000", "BTZ", "", "", -1},
  {"1xxyy10111101001", "CIO", "", "", -1},
  {"1xxyy10111111001", "CIOI", "", "", -1},
  {"1xxyy10011111000", "CLM", "", "", -1},
  {"1101111110101000", "CMP", "", "", -1},
  {"1110111110101000", "CMT", "", "", -1},
  {"1101011110101000", "CMV", "", "", -1},
  {"1xxyy10110001000", "COB", "", "", -1},
  {"1010011111101001", "CRYTC", "", "", -1},
  {"1010011111001001", "CRYTO", "EagleGeneral", "noArguments", EagleGeneral::CRYTO},
  {"1010011111011001", "CRYTZ", "EagleGeneral", "noArguments", EagleGeneral::CRYTZ},
  {"1110011110101000", "CTR", "", "", -1},
  {"111yy11001101001", "CVWN", "EagleCompute", "register", EagleCompute::CVWN},
  {"1xxyy00010001000", "DAD", "", "", -1},
  {"1110011111001001", "DEQUE", "EagleSpecial", "noArguments", EagleSpecial::DEQUE},
  {"1bbb111100bb1001", "DERR", "EagleStack", "bitOffset", EagleStack::DERR},
  {"1nnyy01110001000", "DHXL", "", "", -1},
  {"1nnyy01111001000", "DHXR", "", "", -1},
  {"1101011111001000", "DIV", "EagleCompute", "noArguments", EagleCompute::DIV},
  {"1101111111001000", "DIVS", "", "", -1},
  {"1011111111001000", "DIVX", "EagleCompute", "noArguments", EagleCompute::DIVX},
  {"1xxyy01011001000", "DLSH", "", "", -1},
  {"1xxtt00011001000", "DSB", "", "", -1},
  {"110yy1ii01111000", "DSPA", "", "", -1},
  {"1100011111011001", "DSZTS", "EagleStack", "noArguments", EagleStack::DSZTS},
  {"1111111111001000", "ECLIC", "", "", -1},
  {"1111011110101000", "EDIT", "", "", -1},
  {"100111ii00111000", "EDSZ", "", "", -1},
  {"100101ii00111000", "EISZ", "", "", -1},
  {"100001ii00111000", "EJMP", "", "", -1},
  {"100011ii00111000", "EJSR", "", "", -1},
  {"101yy1ii00111000", "ELDA", "", "", -1},
  {"100yy1ii01111000", "ELDB", "", "", -1},
  {"111yy1ii00111000", "ELEF", "", "", -1},
  {"1100011111101001", "ENQH", "EagleSpecial", "noArguments", EagleSpecial::ENQH},
  {"1100011111111001", "ENQT", "EagleSpecial", "noArguments", EagleSpecial::ENQT},
  {"110yy1ii00111000", "ESTA", "", "", -1},
  {"101yy1ii01111000", "ESTB", "", "", -1},
  {"110yy11000101000", "FAB", "", "", -1},
  {"1xxyy00001101000", "FAD", "EagleFloat", "registerRegister", EagleFloat::FAD},
  {"1iiyy01001101000", "FAMD", "", "", -1},
  {"1iiyy01000101000", "FAMS", "", "", -1},
  {"1xxyy00000101000", "FAS", "EagleFloat", "registerRegister", EagleFloat::FAS},
  {"1101011011101000", "FCLE", "", "", -1},
  {"1xxyy11100101000", "FCMP", "EagleFloat", "registerRegister", EagleFloat::FCMP},
  {"1xxyy00111101000", "FDD", "EagleFloat", "registerRegister", EagleFloat::FDD},
  {"1iiyy01111101000", "FDMD", "", "", -1},
  {"1iiyy01110101000", "FDMS", "", "", -1},
  {"1xxyy00110101000", "FDS", "EagleFloat", "registerRegister", EagleFloat::FDS},
  {"101yy11001101000", "FEXP", "EagleFloat", "register", EagleFloat::FEXP},
  {"1xxyy10110101000", "FFAS", "", "", -1},
  {"1iiyy10111101000", "FFMD", "", "", -1},
  {"111yy11001101000", "FHLV", "EagleFloat", "register", EagleFloat::FHLV},
  {"110yy11001101000", "FINT", "EagleFloat", "register", EagleFloat::FINT},
  {"1xxyy10100101000", "FLAS", "", "", -1},
  {"1iiyy10001101000", "FLDD", "", "", -1},
  {"1iiyy10000101000", "FLDS", "", "", -1},
  {"1iiyy10101101000", "FLMD", "", "", -1},
  {"101ii11011101000", "FLST", "", "", -1},
  {"1xxyy00101101000", "FMD", "EagleFloat", "registerRegister", EagleFloat::FMD},
  {"1iiyy01101101000", "FMMD", "", "", -1},
  {"1iiyy01100101000", "FMMS", "", "", -1},
  {"1xxyy11101101000", "FMOV", "EagleFloat", "registerRegister", EagleFloat::FMOV},
  {"1xxyy00100101000", "FMS", "EagleFloat", "registerRegister", EagleFloat::FMS},
  {"111yy11000101000", "FNEG", "", "", -1},
  {"100yy11000101000", "FNOM", "", "", -1},
  {"1000011010101000", "FNS", "", "", -1},
  {"1110111011101000", "FPOP", "", "", -1},
  {"1110011011101000", "FPSH", "", "", -1},
  {"1xxyy10011011000", "FRDS", "EagleFloat", "registerRegister", EagleFloat::FRDS},
  {"101yy11000101000", "FRH", "EagleFloat", "register", EagleFloat::FRH},
  {"1000111010101000", "FSA", "", "", -1},
  {"100yy11001101000", "FSCAL", "EagleFloat", "register", EagleFloat::FSCAL},
  {"1xxyy00011101000", "FSD", "EagleFloat", "registerRegister", EagleFloat::FSD},
  {"1001011010101000", "FSEQ", "EagleFloat", "noArguments", EagleFloat::FSEQ},
  {"1010111010101000", "FSGE", "EagleFloat", "noArguments", EagleFloat::FSGE},
  {"1011111010101000", "FSGT", "EagleFloat", "noArguments", EagleFloat::FSGT},
  {"1011011010101000", "FSLE", "EagleFloat", "noArguments", EagleFloat::FSLE},
  {"1010011010101000", "FSLT", "EagleFloat", "noArguments", EagleFloat::FSLT},
  {"1iiyy01011101000", "FSMD", "", "", -1},
  {"1iiyy01010101000", "FSMS*", "", "", -1},
  {"1100111010101000", "FSND*", "", "", -1},
  {"1001111010101000", "FSNE", "EagleFloat", "noArguments", EagleFloat::FSNE},
  {"1111111010101000", "FSNER", "", "", -1},
  {"1100011010101000", "FSNM", "", "", -1},
  {"1110011010101000", "FSNO*", "", "", -1},
  {"1110111010101000", "FSNOD", "", "", -1},
  {"1101011010101000", "FSNU", "", "", -1},
  {"1101111010101000", "FSNUD", "", "", -1},
  {"1111011010101000", "FSNUO", "", "", -1},
  {"1xxyy00010101000", "FSS", "EagleFloat", "registerRegister", EagleFloat::FSS},
  {"100ii11011101000", "FSST", "", "", -1},
  {"1iiyy10011101000", "FSTD", "", "", -1},
  {"1iiyy10010101000", "FSTS", "", "", -1},
  {"1100111011101000", "FTD", "EagleFloat", "noArguments", EagleFloat::FTD},
  {"1100011011101000", "FTE", "EagleFloat", "noArguments", EagleFloat::FTE},
  {"1010011101111001", "FXTD", "", "", -1},
  {"1100011101001001", "FXTE", "", "", -1},
  {"110yy11011111000", "HLV", "", "", -1},
  {"1nnyy01100001000", "HXL", "", "", -1},
  {"1nnyy01101001000", "HXR", "", "", -1},
  {"1xxyy00100001000", "IOR", "", "", -1},
  {"100yy11111111000", "IORI", "", "", -1},
  {"1100011111001001", "ISZTS", "EagleStack", "noArguments", EagleStack::ISZTS},
  {"101ii11011001001", "LCALL", "EagleStack", "wideIndirectArgument", EagleStack::LCALL},
  {"1000011101011001", "LCPID", "", "", -1},
  {"110yy11001101001", "LDAFP", "EagleStack", "register", EagleStack::LDAFP},
  {"110yy11001001001", "LDASB", "EagleStack", "register", EagleStack::LDASB},
  {"101yy11001101001", "LDASL", "EagleStack", "register", EagleStack::LDASL},
  {"101yy11001001001", "LDASP", "EagleStack", "register", EagleStack::LDASP},
  {"100yy11001001001", "LDATS", "EagleStack", "register", EagleStack::LDATS},
  {"1xxyy10111001000", "LDB", "", "", -1},
  {"100yy11110101000", "LDI", "", "", -1},
  {"1100011110101000", "LDIX", "", "", -1},
  {"1iiyy10100011001", "LDSP", "EagleGeneral", "registerWideIndirect", EagleGeneral::LDSP},
  {"1iiyy00011011001", "LFAMD", "EagleFloat", "registerWideIndirect", EagleFloat::LFAMD},
  {"1iiyy00011001001", "LFAMS", "EagleFloat", "registerWideIndirect", EagleFloat::LFAMS},
  {"1iiyy00111111001", "LFDMD", "", "", -1},
  {"1iiyy00111101001", "LFDMS", "", "", -1},
  {"1iiyy01011011001", "LFLDD", "EagleFloat", "registerWideIndirect", EagleFloat::LFLDD},
  {"1iiyy01011001001", "LFLDS", "EagleFloat", "registerWideIndirect", EagleFloat::LFLDS},
  {"110ii11011011001", "LFLST", "", "", -1},
  {"1iiyy00111011001", "LFMMD", "EagleFloat", "registerWideIndirect", EagleFloat::LFMMD},
  {"1iiyy00111001001", "LFMMS", "EagleFloat", "registerWideIndirect", EagleFloat::LFMMS},
  {"1iiyy00011111001", "LFSMD*", "", "", -1},
  {"1iiyy00011101001", "LFSMS*", "", "", -1},
  {"110ii11011101001", "LFSST*", "", "", -1},
  {"1iiyy01011111001", "LFSTD", "EagleFloat", "registerWideIndirect", EagleFloat::LFSTD},
  {"1iiyy01011101001", "LFSTS", "EagleFloat", "registerWideIndirect", EagleFloat::LFSTS},
  {"101ii11011011001", "LJMP", "EagleGeneral", "wideIndirect", EagleGeneral::LJMP},
  {"101ii11011101001", "LJSR", "EagleGeneral", "wideIndirect", EagleGeneral::LJSR},
  {"1iiyy10011001001", "LLDB", "EagleGeneral", "registerWideByteIndexed", EagleGeneral::LLDB},
  {"1iiyy01111101001", "LLEF", "EagleGeneral", "registerWideIndirect", EagleGeneral::LLEF},
  {"1iiyy10011101001", "LLEFB", "EagleGeneral", "registerWideIndirect", EagleGeneral::LLEFB},
  {"1000011111001001", "LMRF", "", "", -1},
  {"1iiyy01000011000", "LNADD", "EagleCompute", "registerWideIndirect", EagleCompute::LNADD},
  {"1nnii11000011000", "LNADI", "EagleCompute", "tinyImmediateWordIndirect", EagleCompute::LNADI},
  {"1iiyy01011011000", "LNDIV", "", "", -1},
  {"1yyii11010011000", "LNDO", "EagleGeneral", "wideIndirectArgument", EagleGeneral::LNDO},
  {"100ii11011011001", "LNDSZ", "", "", -1},
  {"100ii11011001001", "LNISZ", "", "", -1},
  {"1iiyy01111001001", "LNLDA", "EagleGeneral", "registerWideIndirect", EagleGeneral::LNLDA},
  {"1iiyy01010011000", "LNMUL", "EagleCompute", "registerWideIndirect", EagleCompute::LNMUL},
  {"1nnii11001011000", "LNSBI", "EagleCompute", "tinyImmediateWordIndirect", EagleCompute::LNSBI},
  {"1iiyy01111011001", "LNSTA", "EagleGeneral", "registerWideIndirect", EagleGeneral::LNSTA},
  {"1iiyy01001011000", "LNSUB", "EagleCompute", "registerWideIndirect", EagleCompute::LNSUB},
  {"1xxyy10100001000", "LOB", "", "", -1},
  {"101ii11011111001", "LPEF", "EagleStack", "wideIndirect", EagleStack::LPEF},
  {"110ii11011111001", "LPEFB", "EagleStack", "wideIndirect", EagleStack::LPEFB},
  {"1000011111101001", "LPHY", "", "", -1},
  {"110ii11011001001", "LPSHJ", "EagleStack", "wideIndirect", EagleStack::LPSHJ},
  {"1010011110011001", "LPSR", "EagleGeneral", "noArguments", EagleGeneral::LPSR},
  {"1110011100111001", "LPTE", "", "", -1},
  {"1xxyy10101001000", "LRB", "", "", -1},
  {"1100011110111001", "LSBRA", "", "", -1},
  {"1110011110001001", "LSBRS", "", "", -1},
  {"1xxyy01010001000", "LSH", "", "", -1},
  {"1111111110101000", "LSN", "", "", -1},
  {"1iiyy10011011001", "LSTB", "EagleGeneral", "registerWideByteIndexed", EagleGeneral::LSTB},
  {"1iiyy01100011000", "LWADD", "EagleCompute", "registerWideIndirect", EagleCompute::LWADD},
  {"1nnii11100011000", "LWADI", "", "", -1},
  {"1iiyy01111011000", "LWDIV", "", "", -1},
  {"1yyii11110011000", "LWDO", "EagleGeneral", "wideIndirectArgument", EagleGeneral::LWDO},
  {"100ii11011111001", "LWDSZ", "", "", -1},
  {"100ii11011101001", "LWISZ", "", "", -1},
  {"1iiyy01111111001", "LWLDA", "EagleGeneral", "registerWideIndirect", EagleGeneral::LWLDA},
  {"1iiyy01110011000", "LWMUL", "EagleCompute", "registerWideIndirect", EagleCompute::LWMUL},
  {"1nnii11101011000", "LWSBI", "", "", -1},
  {"1iiyy10011111001", "LWSTA", "EagleGeneral", "registerWideIndirect", EagleGeneral::LWSTA},
  {"1iiyy01101011000", "LWSUB", "EagleCompute", "registerWideIndirect", EagleCompute::LWSUB},
  {"100yy11011111000", "MSP", "", "", -1},
  {"1100011111001000", "MUL", "", "", -1},
  {"1100111111001000", "MULS", "", "", -1},
  {"1xxyy00001001001", "NADD", "EagleCompute", "registerRegister", EagleCompute::NADD},
  {"110yy11000111001", "NADDI", "EagleCompute", "registerWordImmediate", EagleCompute::NADDI},
  {"1nnyy10110011001", "NADI", "EagleCompute", "tinyImmediateRegister", EagleCompute::NADI},
  {"1xxyy00001111001", "NDIV", "", "", -1},
  {"110yy11000101001", "NLDAI", "EagleCompute", "wordImmediateRegister", EagleCompute::NLDAI},
  {"1xxyy00001101001", "NMUL", "EagleCompute", "registerRegister", EagleCompute::NMUL},
  {"1xxyy10100001001", "NNEG", "EagleCompute", "registerRegister", EagleCompute::NNEG},
  {"111yy11000001001", "NSALA", "", "", -1},
  {"111yy11000011001", "NSALM", "", "", -1},
  {"111yy11000101001", "NSANA", "EagleCompute", "registerWordImmediate", EagleCompute::NSANA},
  {"111yy11000111001", "NSANM", "", "", -1},
  {"1nnyy10110101001", "NSBI", "EagleCompute", "tinyImmediateRegister", EagleCompute::NSBI},
  {"1xxyy00001011001", "NSUB", "EagleCompute", "registerRegister", EagleCompute::NSUB},
  {"1110011110111001", "ORFB", "", "", -1},
  {"1110011110011001", "PATU*", "", "", -1},
  {"1000011101001001", "PBX*", "", "", -1},
  {"1xxyy10111011001", "PIO", "", "", -1},
  {"1xxyy11010001000", "POP", "", "", -1},
  {"1000111111001000", "POPB", "", "", -1},
  {"1001111111001000", "POPJ", "", "", -1},
  {"1xxyy11001001000", "PSH", "", "", -1},
  {"100001ii10111000", "PSHJ", "", "", -1},
  {"1000011111001000", "PSHR", "", "", -1},
  {"1110011110101001", "RRFB", "", "", -1},
  {"1110111111001000", "RSTR", "", "", -1},
  {"1010111111001000", "RTN", "", "", -1},
  {"1110011111001000", "SAVE", "", "", -1},
  {"1010011111001000", "SAVZ", "", "", -1},
  {"1nnyy00001001000", "SBI", "", "", -1},
  {"1xxyy01101001001", "SEX", "EagleCompute", "registerRegister", EagleCompute::SEX},
  {"1xxyy01001001000", "SGE", "", "", -1},
  {"1xxyy01000001000", "SGT", "", "", -1},
  {"1000011111011001", "SMRF", "", "", -1},
  {"1xxyy10111111000", "SNB", "", "", -1},
  {"1010011110111001", "SNOVR", "", "", -1},
  {"1010011110101001", "SPSR", "", "", -1},
  {"1110011100101001", "SPTE", "", "", -1},
  {"1110011111011001", "SSPT", "", "", -1},
  {"110yy11001111001", "STAFP", "EagleStack", "register", EagleStack::STAFP},
  {"110yy11001011001", "STASB", "EagleStack", "register", EagleStack::STASB},
  {"101yy11001111001", "STASL", "EagleStack", "register", EagleStack::STASL},
  {"101yy11001011001", "STASP", "EagleStack", "register", EagleStack::STASP},
  {"100yy11001011001", "STATS", "EagleStack", "register", EagleStack::STATS},
  {"1xxyy11000001000", "STB", "", "", -1},
  {"101yy11110101000", "STI", "", "", -1},
  {"1100111110101000", "STIX", "", "", -1},
  {"1xxyy10010001000", "SZB", "", "", -1},
  {"1xxyy10011001000", "SZBO", "", "", -1},
  {"1100011110011001", "VBP", "", "", -1},
  {"1100011110101001", "VWP", "", "", -1},
  {"1xxyy01001001001", "WADC", "EagleCompute", "registerRegister", EagleCompute::WADC},
  {"1xxyy00101001001", "WADD", "EagleCompute", "registerRegister", EagleCompute::WADD},
  {"100yy11010001001", "WADDI", "EagleCompute", "registerWideImmediate", EagleCompute::WADDI},
  {"1nnyy10010111001", "WADI", "EagleCompute", "tinyImmediateRegister", EagleCompute::WADI},
  {"1xxyy10101001001", "WANC", "", "", -1},
  {"1xxyy10001001001", "WAND", "EagleCompute", "registerRegister", EagleCompute::WAND},
  {"100yy11010011001", "WANDI", "EagleCompute", "registerWideImmediate", EagleCompute::WANDI},
  {"1xxyy01001111001", "WASH", "", "", -1},
  {"110yy11010101001", "WASHI", "", "", -1},
  {"1110011101001001", "WBLM", "EagleSpecial", "noArguments", EagleSpecial::WBLM},
  {"1dddd0dddd111000", "WBR", "EagleGeneral", "shortDisplacement", EagleGeneral::WBR},
  {"1100011100011001", "QSEARCH*", "", "", -1},
  {"1xxyy01010011001", "WBTO", "EagleCompute", "registerRegister", EagleCompute::WBTO},
  {"1xxyy01010101001", "WBTZ", "EagleCompute", "registerRegister", EagleCompute::WBTZ},
  {"1xxyy10101101001", "WCLM", "EagleGeneral", "registerRegister", EagleGeneral::WCLM},
  {"1010011101011001", "WCMP", "EagleSpecial", "noArguments", EagleSpecial::WCMP},
  {"1010011101001001", "WCMT", "", "", -1},
  {"1000011101111001", "WCMV", "EagleSpecial", "noArguments", EagleSpecial::WCMV},
  {"1xxyy10010001001", "WCOB", "", "", -1},
  {"1xxyy10001011001", "WCOM", "EagleCompute", "registerRegister", EagleCompute::WCOM},
  {"1110011100001001", "WCST", "EagleSpecial", "noArguments", EagleSpecial::WCST},
  {"1000011101101001", "WCTR*", "", "", -1},
  {"1000011100011001", "W_DEC", "", "", -1},
  {"1xxyy00101111001", "WDIV", "EagleCompute", "registerRegister", EagleCompute::WDIV},
  {"1110011101101001", "WDIVS", "EagleCompute", "noArguments", EagleCompute::WDIVS},
  {"1000011111111001", "WDPOP", "", "", -1},
  {"1010011101101001", "WEDIT", "", "", -1},
  {"1xxyy10010011001", "WFFAD", "EagleFloat", "registerRegister", EagleFloat::WFFAD},
  {"1xxyy10010101001", "WFLAD", "EagleFloat", "registerRegister", EagleFloat::WFLAD},
  {"1010011110001001", "WFPOP", "EagleStack", "noArguments", EagleStack::WFPOP},
  {"1000011110111001", "WFPSH", "EagleStack", "noArguments", EagleStack::WFPSH},
  {"1000111001111001", "FP_INTR", "", "", -1},
  {"1000111001101001", "GRAPHICS", "", "", -1},
  {"111yy11001011001", "WHLV", "EagleCompute", "register", EagleCompute::WHLV},
  {"1xxyy01001011001", "WINC", "EagleCompute", "registerRegister", EagleCompute::WINC},
  {"1xxyy10001101001", "WIOR", "EagleCompute", "registerRegister", EagleCompute::WIOR},
  {"100yy11010101001", "WIORI", "EagleCompute", "registerWideImmediate", EagleCompute::WIORI},
  {"110yy11010001001", "WLDAI", "EagleCompute", "wideImmediate", EagleCompute::WLDAI},
  {"1xxyy10100101001", "WLDB", "EagleGeneral", "registerRegister", EagleGeneral::WLDB},
  {"111yy11001111001", "WLDI", "", "", -1},
  {"1100011101011001", "WLDIX", "", "", -1},
  {"1010011111111001", "WLMP", "", "", -1},
  {"1xxyy01110101001", "WLOB", "EagleCompute", "registerRegister", EagleCompute::WLOB},
  {"1xxyy01110111001", "WLRB", "", "", -1},
  {"1xxyy10101011001", "WLSH", "EagleCompute", "registerRegister", EagleCompute::WLSH},
  {"111yy11011011001", "WLSHI", "EagleCompute", "registerWordImmediate", EagleCompute::WLSHI},
  {"1nnyy10110111001", "WLSI", "EagleCompute", "tinyImmediateRegister", EagleCompute::WLSI},
  {"1100011101111001", "WLSN", "", "", -1},
  {"1110011100011001", "WMESS", "EagleSpecial", "noArguments", EagleSpecial::WMESS},
  {"1xxyy01101111001", "WMOV", "EagleCompute", "registerRegister", EagleCompute::WMOV},
  {"111yy11010011001", "WMOVR", "EagleCompute", "register", EagleCompute::WMOVR},
  {"111yy11001001001", "WMSP", "EagleStack", "register", EagleStack::WMSP},
  {"1xxyy00101101001", "WMUL", "EagleCompute", "registerRegister", EagleCompute::WMUL},
  {"1110011101011001", "WMULS", "", "", -1},
  {"111yy11011111001", "WNADI", "EagleCompute", "registerWordImmediate", EagleCompute::WNADI},
  {"1xxyy01001101001", "WNEG", "EagleCompute", "registerRegister", EagleCompute::WNEG},
  {"1xxyy00010001001", "WPOP", "EagleStack", "registerRegister", EagleStack::WPOP},
  {"1110011101111001", "WPOPB", "EagleStack", "noArguments", EagleStack::WPOPB},
  {"1000011110001001", "WPOPJ", "EagleStack", "noArguments", EagleStack::WPOPJ},
  {"1xxyy10101111001", "WPSH", "EagleStack", "registerRegister", EagleStack::WPSH},
  {"1000011110011001", "WRSTR", "", "", -1},
  {"1000011110101001", "WRTN", "EagleStack", "noArguments", EagleStack::WRTN},
  {"101yy11010011001", "WSALA", "", "", -1},
  {"101yy11010111001", "WSALM", "", "", -1},
  {"101yy11010001001", "WSANA", "EagleCompute", "registerWideImmediate", EagleCompute::WSANA},
  {"101yy11010101001", "WSANM", "", "", -1},
  {"1010011100101001", "WSAVR", "EagleStack", "wordImmediate", EagleStack::WSAVR},
  {"1010011100111001", "WSAVS", "EagleStack", "wordImmediate", EagleStack::WSAVS},
  {"1nnyy10110001001", "WSBI", "EagleCompute", "tinyImmediateRegister", EagleCompute::WSBI},
  {"1xxyy00010111001", "WSEQ", "EagleCompute", "registerRegister", EagleCompute::WSEQ},
  {"111yy11011001001", "WSEQI", "EagleCompute", "registerWordImmediate", EagleCompute::WSEQI},
  {"1xxyy00110011001", "WSGE", "EagleCompute", "registerRegister", EagleCompute::WSGE},
  {"1xxyy00110111001", "WSGT", "EagleCompute", "registerRegister", EagleCompute::WSGT},
  {"111yy11010001001", "WSGTI", "EagleCompute", "registerWordImmediate", EagleCompute::WSGTI},
  {"1bbb111101bb1001", "WSKBO", "EagleCompute", "bitPosition", EagleCompute::WSKBO},
  {"1bbb111110bb1001", "WSKBZ", "EagleCompute", "bitPosition", EagleCompute::WSKBZ},
  {"1xxyy00110101001", "WSLE", "EagleCompute", "registerRegister", EagleCompute::WSLE},
  {"111yy11010101001", "WSLEI", "EagleCompute", "registerWordImmediate", EagleCompute::WSLEI},
  {"1xxyy01010001001", "WSLT", "EagleCompute", "registerRegister", EagleCompute::WSLT},
  {"1xxyy01110001001", "WSNB", "", "", -1},
  {"1xxyy00110001001", "WSNE", "EagleCompute", "registerRegister", EagleCompute::WSNE},
  {"111yy11011101001", "WSNEI", "EagleCompute", "registerWordImmediate", EagleCompute::WSNEI},
  {"1000011100101001", "WSSVR", "EagleStack", "wordImmediate", EagleStack::WSSVR},
  {"1000011100111001", "WSSVS", "EagleStack", "wordImmediate", EagleStack::WSSVS},
  {"1xxyy10100111001", "WSTB", "EagleGeneral", "registerRegister", EagleGeneral::WSTB},
  {"111yy11010111001", "WSTI", "", "", -1},
  {"1100011101101001", "WSTIX", "", "", -1},
  {"1xxyy00101011001", "WSUB", "EagleCompute", "registerRegister", EagleCompute::WSUB},
  {"1xxyy01010111001", "WSZB", "EagleCompute", "registerRegister", EagleCompute::WSZB},
  {"1xxyy01110011001", "WSZBO", "EagleCompute", "registerRegister", EagleCompute::WSZBO},
  {"110yy11010011001", "WUGTI", "EagleCompute", "registerWideImmediate", EagleCompute::WUGTI},
  {"110yy11010111001", "WULEI", "EagleCompute", "registerWideImmediate", EagleCompute::WULEI},
  {"1xxyy00010011001", "WUSGE", "EagleCompute", "registerRegister", EagleCompute::WUSGE},
  {"1xxyy00010101001", "WUSGT", "EagleCompute", "registerRegister", EagleCompute::WUSGT},
  {"1xxyy01101101001", "WXCH", "EagleCompute", "registerRegister", EagleCompute::WXCH},
  {"1010011100001001", "WXOP", "", "", -1},
  {"1xxyy10001111001", "WXOR", "EagleCompute", "registerRegister", EagleCompute::WXOR},
  {"100yy11010111001", "WXORI", "EagleCompute", "registerWideImmediate", EagleCompute::WXORI},
  {"100ii11000001001", "XCALL", "EagleStack", "wordIndirectArgument", EagleStack::XCALL},
  {"1xxyy00111001000", "XCH", "", "", -1},
  {"101yy11011111000", "XCT", "EagleGeneral", "register", EagleGeneral::XCT},
  {"1iiyy00000011001", "XFAMD", "EagleFloat", "registerWordIndirect", EagleFloat::XFAMD},
  {"1iiyy00000001001", "XFAMS", "EagleFloat", "registerWordIndirect", EagleFloat::XFAMS},
  {"1iiyy00100111001", "XFDMD", "", "", -1},
  {"1iiyy00100101001", "XFDMS", "", "", -1},
  {"1iiyy01000011001", "XFLDD", "EagleFloat", "registerWordIndirect", EagleFloat::XFLDD},
  {"1iiyy01000001001", "XFLDS", "EagleFloat", "registerWordIndirect", EagleFloat::XFLDS},
  {"1iiyy00000111001", "XFMMD", "EagleFloat", "registerWordIndirect", EagleFloat::XFMMD},
  {"1iiyy00000101001", "XFMMS", "EagleFloat", "registerWordIndirect", EagleFloat::XFMMS},
  {"1iiyy00100011001", "XFSMD*", "", "", -1},
  {"1iiyy00100001001", "XFSMS", "", "", -1},
  {"1iiyy01000111001", "XFSTD", "EagleFloat", "registerWordIndirect", EagleFloat::XFSTD},
  {"1iiyy01000101001", "XFSTS", "EagleFloat", "registerWordIndirect", EagleFloat::XFSTS},
  {"110ii11000001001", "XJMP", "EagleGeneral", "wordIndirect", EagleGeneral::XJMP},
  {"110ii11000011001", "XJSR", "EagleGeneral", "wordIndirect", EagleGeneral::XJSR},
  {"1iiyy10000011001", "XLDB", "EagleGeneral", "registerWordByteIndexed", EagleGeneral::XLDB},
  {"1iiyy10000001001", "XLEF", "EagleGeneral", "registerWordIndirect", EagleGeneral::XLEF},
  {"1iiyy10000111001", "XLEFB", "EagleGeneral", "registerWordByteIndexed", EagleGeneral::XLEFB},
  {"1iiyy00000011000", "XNADD", "EagleCompute", "registerWordIndirect", EagleCompute::XNADD},
  {"1nnii10000011000", "XNADI", "EagleCompute", "tinyImmediateWordIndirect", EagleCompute::XNADI},
  {"1iiyy00011011000", "XNDIV", "", "", -1},
  {"1yyii10010011000", "XNDO", "EagleGeneral", "wordIndirectArgument", EagleGeneral::XNDO},
  {"101ii11000001001", "XNDSZ", "EagleCompute", "wordIndirect", EagleCompute::XNDSZ},
  {"100ii11000111001", "XNISZ", "EagleCompute", "wordIndirect", EagleCompute::XNISZ},
  {"1iiyy01100101001", "XNLDA", "EagleGeneral", "registerWordIndirect", EagleGeneral::XNLDA},
  {"1iiyy00010011000", "XNMUL", "EagleCompute", "registerWordIndirect", EagleCompute::XNMUL},
  {"1nnii10001011000", "XNSBI", "EagleCompute", "tinyImmediateWordIndirect", EagleCompute::XNSBI},
  {"1iiyy01100111001", "XNSTA", "EagleGeneral", "registerWordIndirect", EagleGeneral::XNSTA},
  {"1iiyy00001011000", "XNSUB", "EagleCompute", "registerWordIndirect", EagleCompute::XNSUB},
  {"1xxyy11011001000", "XOP0", "", "", -1},
  {"1xxyy00101001000", "XOR", "", "", -1},
  {"101yy11111111000", "XORI", "", "", -1},
  {"100ii11000101001", "XPEF", "EagleStack", "wordIndirect", EagleStack::XPEF},
  {"101ii11000101001", "XPEFB", "EagleStack", "wordIndirect", EagleStack::XPEFB},
  {"100ii11000011001", "XPSHJ", "EagleStack", "wordIndirect", EagleStack::XPSHJ},
  {"1iiyy10000101001", "XSTB", "EagleGeneral", "registerWordByteIndexed", EagleGeneral::XSTB},
  {"1100011100001001", "XVCT", "", "", -1},
  {"1iiyy00100011000", "XWADD", "EagleCompute", "registerWordIndirect", EagleCompute::XWADD},
  {"1nnii10100011000", "XWADI", "EagleCompute", "tinyImmediateWordIndirect", EagleCompute::XWADI},
  {"1iiyy00111011000", "XWDIV", "", "", -1},
  {"1yyii10110011000", "XWDO", "EagleGeneral", "wordIndirectArgument", EagleGeneral::XWDO},
  {"101ii11000111001", "XWDSZ", "", "", -1},
  {"101ii11000011001", "XWISZ", "EagleCompute", "wordIndirect", EagleCompute::XWISZ},
  {"1iiyy01100001001", "XWLDA", "EagleGeneral", "registerWordIndirect", EagleGeneral::XWLDA},
  {"1iiyy00110011000", "XWMUL", "EagleCompute", "registerWordIndirect", EagleCompute::XWMUL},
  {"1nnii10101011000", "XWSBI", "EagleCompute", "tinyImmediateWordIndirect", EagleCompute::XWSBI},
  {"1iiyy01100011001", "XWSTA", "EagleGeneral", "registerWordIndirect", EagleGeneral::XWSTA},
  {"1iiyy00101011000", "XWSUB", "EagleCompute", "registerWordIndirect", EagleCompute::XWSUB},
  {"1xxyy01101011001", "ZEX", "EagleCompute", "registerRegister", EagleCompute::ZEX}
};

uint32_t Decoder::mask_for_match(const std::string& match) {
  if(match.length()!=16)
    throw std::runtime_error("Invalid instruction mask: " + match);
  uint32_t mask=0;
  for(int i=0;i<16;i++) {
    mask=mask*2;
    if(match[i]=='0' || match[i]=='1')
      mask++;
  }
  return mask;
}

uint32_t Decoder::value_for_match(const std::string& match) {
  if(match.length()!=16)
    throw std::runtime_error("Invalid instruction mask: " + match);
  uint32_t value=0;
  for(int i=0;i<16;i++) {
    value=value*2;
    if(match[i]=='1')
      value++;
  }
  return value;
}

Instruction* Decoder::instantiate(const std::string& class_name) {
  if(class_name=="NovaCompute") return new NovaCompute();
  if(class_name=="EagleCompute") return new EagleCompute();
  if(class_name=="EagleGeneral") return new EagleGeneral();
  if(class_name=="EagleStack") return new EagleStack();
  if(class_name=="EagleFloat") return new EagleFloat();
  if(class_name=="EagleSpecial") return new EagleSpecial();
  throw std::runtime_error("Unknown instruction class: " + class_name);
}

Instruction* Decoder::find_opcode(const std::vector<Definition>& table, uint32_t opcode) {
  Instruction* instruction=nullptr;
  for(size_t index=0;index<table.size();index++) {
    uint32_t mask=mask_for_match(table[index].match);
    uint32_t value=value_for_match(table[index].match);
    if((opcode & mask)==value) {
      if(instruction!=nullptr) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Instruction is not unique for opcode %04X", opcode);
        throw std::runtime_error(buf);
      }
      if(table[index].instruction_class.empty())
        instruction=new Instruction();
      else
        instruction=instantiate(table[index].instruction_class);
      instruction->setup(opcode, table[index].name, table[index].instruction_format, table[index].oper);
    }
  }
  return instruction;
}

Instruction* Decoder::slow_decode(bool lef_mode, uint32_t opcode) {
  if(opcode<0x6000)
    return find_opcode(nova_general_opcodes, opcode);
  if(opcode<0x8000)
    return find_opcode(lef_mode ? nova_lef_opcodes : nova_io_opcodes, opcode);
  uint32_t bottom=opcode & 0x0F;
  return find_opcode((bottom==8 || bottom==9) ? eclipse_mv_opcodes : nova_compute_opcodes, opcode);
}

Instruction* Decoder::decode(bool lef_mode, uint32_t opcode) {
  if(opcode<0x6000)
    return nova_general[opcode>>11];
  if(opcode<0x8000) {
    if(lef_mode)
      return &lef_instruction;
    return nova_io[(opcode>>8) & 0x1F];
  }
  uint32_t bottom=opcode & 0x0F;
  if(bottom!=8 && bottom!=9)
    return nova_compute[(opcode>>8) & 0x07];
  return eclipse_mv[((opcode & 0x7FF0)>>3) + (opcode & 0x01)];
}

void Decoder::initialize() {
  int index;

  // Build nova general table
  index=0;
  for(uint32_t opcode=0x0000;opcode<0x6000;opcode+=0x800)
    nova_general[index++]=slow_decode(false, opcode);

  // Build nova IO table
  index=0;
  for(uint32_t opcode=0x6000;opcode<0x8000;opcode+=0x100)
    nova_io[index++]=slow_decode(true, opcode);

  // Build nova compute table
  index=0;
  for(uint32_t opcode=0x8000;opcode<0x8800;opcode+=0x100)
    nova_compute[index++]=slow_decode(false, opcode);

  // Build eclipse MV table
  index=0;
  for(uint32_t opcode=0x8000;opcode<0x10000;opcode+=16) {
    eclipse_mv[index++]=slow_decode(false, opcode+8);
    eclipse_mv[index++]=slow_decode(false, opcode+9);
  }
}

} // namespace hw
