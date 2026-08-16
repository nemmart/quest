package hw;

public class Decoder {

// asterisk in the instruction name means the instruction occurs somewhere in the QUEST.PR binary

   static public Definition[] novaGeneralOpcodes=new Definition[]{
     new Definition("00000piixxxxxxxx", "JMP",      null, null, -1),
     new Definition("00001piixxxxxxxx", "JSR",      null, null, -1),
     new Definition("00010piixxxxxxxx", "ISZ",      null, null, -1),
     new Definition("00011piixxxxxxxx", "DSZ",      null, null, -1),
     new Definition("001yypiixxxxxxxx", "LDA",      null, null, -1),
     new Definition("010yyaiixxxxxxxx", "STA",      null, null, -1)
   };

   static public Definition[] novaIOOpcodes=new Definition[]{
     new Definition("01100000ffdddddd", "NIO",      null, null, -1),  // IOInstruction
     new Definition("0110100000111111", "NCLID",    null, null, -1),  // IOInstruction
     new Definition("011yy001ffdddddd", "DIA",      null, null, -1),  // IOInstruction
     new Definition("011yy011ffdddddd", "DIB",      null, null, -1),  // IOInstruction
     new Definition("011yy101ffdddddd", "DIC",      null, null, -1),  // IOInstruction
     new Definition("011yy010ffdddddd", "DOA",      null, null, -1),  // IOInstruction
     new Definition("011yy100ffdddddd", "DOB",      null, null, -1),  // IOInstruction
     new Definition("011yy110ffdddddd", "DOC",      null, null, -1),  // IOInstruction
     new Definition("011yy101ff111111", "IORST",    null, null, -1)   // IOInstruction
   };

   static public Definition[] novaLEFOpcodes=new Definition[]{
     new Definition("011yyaiixxxxxxxx", "LEF*",      null, null, -1)
   };

   static public Definition[] novaComputeOpcodes=new Definition[]{
     new Definition("1xxyy000ssccnkkk", "COM",      "NovaCompute", "novaCompute", NovaCompute.COM),
     new Definition("1xxyy001ssccnkkk", "NEG",      "NovaCompute", "novaCompute", NovaCompute.NEG),
     new Definition("1xxyy010ssccnkkk", "MOV",      "NovaCompute", "novaCompute", NovaCompute.MOV),
     new Definition("1xxyy011ssccnkkk", "INC",      "NovaCompute", "novaCompute", NovaCompute.INC),
     new Definition("1xxyy100ssccnkkk", "ADC",      "NovaCompute", "novaCompute", NovaCompute.ADC),
     new Definition("1xxyy101ssccnkkk", "SUB",      "NovaCompute", "novaCompute", NovaCompute.SUB),
     new Definition("1xxyy110ssccnkkk", "ADD",      "NovaCompute", "novaCompute", NovaCompute.ADD),
     new Definition("1xxyy111ssccnkkk", "AND",      "NovaCompute", "novaCompute", NovaCompute.AND)
   };

   static public Definition[] eclipseMVOpcodes=new Definition[]{
     new Definition("111yy11111111000", "ADDI",     "EagleCompute", "registerWordImmediate", EagleCompute.ADDI),
     new Definition("1nnyy00000001000", "ADI",      null, null, -1),
     new Definition("1xxyy00110001000", "ANC",      null, null, -1),
     new Definition("110yy11111111000", "ANDI",     "EagleCompute", "registerWordImmediate", EagleCompute.ANDI),
     new Definition("1001011111001000", "BAM",      null, null, -1),
     new Definition("1100011110001001", "BKPT*",     null, null, -1),
     new Definition("1011011111001000", "BLM",      null, null, -1),
     new Definition("1xxyy10000001000", "BTO",      null, null, -1),
     new Definition("1xxyy10001001000", "BTZ",      null, null, -1),
     new Definition("1xxyy10111101001", "CIO",      null, null, -1),

     new Definition("1xxyy10111111001", "CIOI",     null, null, -1),
     new Definition("1xxyy10011111000", "CLM",      null, null, -1),
     new Definition("1101111110101000", "CMP",      null, null, -1),
     new Definition("1110111110101000", "CMT",      null, null, -1),
     new Definition("1101011110101000", "CMV",      null, null, -1),
     new Definition("1xxyy10110001000", "COB",      null, null, -1),
     new Definition("1010011111101001", "CRYTC",    null, null, -1),
     new Definition("1010011111001001", "CRYTO",    "EagleGeneral", "noArguments", EagleGeneral.CRYTO),
     new Definition("1010011111011001", "CRYTZ",    "EagleGeneral", "noArguments", EagleGeneral.CRYTZ),
     new Definition("1110011110101000", "CTR",      null, null, -1),
     new Definition("111yy11001101001", "CVWN",     "EagleCompute", "register", EagleCompute.CVWN),
     new Definition("1xxyy00010001000", "DAD",      null, null, -1),

     new Definition("1110011111001001", "DEQUE",    "EagleSpecial", "noArguments", EagleSpecial.DEQUE),
     new Definition("1bbb111100bb1001", "DERR",     "EagleStack", "bitOffset", EagleStack.DERR),
     new Definition("1nnyy01110001000", "DHXL",     null, null, -1),
     new Definition("1nnyy01111001000", "DHXR",     null, null, -1),

     new Definition("1101011111001000", "DIV",      "EagleCompute", "noArguments", EagleCompute.DIV),
     new Definition("1101111111001000", "DIVS",     null, null, -1),
     new Definition("1011111111001000", "DIVX",     "EagleCompute", "noArguments", EagleCompute.DIVX),
     new Definition("1xxyy01011001000", "DLSH",     null, null, -1),

     new Definition("1xxtt00011001000", "DSB",      null, null, -1),
     new Definition("110yy1ii01111000", "DSPA",     null, null, -1),
     new Definition("1100011111011001", "DSZTS",    "EagleStack", "noArguments", EagleStack.DSZTS),
     new Definition("1111111111001000", "ECLIC",    null, null, -1),
     new Definition("1111011110101000", "EDIT",     null, null, -1),
     new Definition("100111ii00111000", "EDSZ",     null, null, -1),
     new Definition("100101ii00111000", "EISZ",     null, null, -1),
     new Definition("100001ii00111000", "EJMP",     null, null, -1),
     new Definition("100011ii00111000", "EJSR",     null, null, -1),
     new Definition("101yy1ii00111000", "ELDA",     null, null, -1),
     new Definition("100yy1ii01111000", "ELDB",     null, null, -1),
     new Definition("111yy1ii00111000", "ELEF",     null, null, -1),
     new Definition("1100011111101001", "ENQH",     null, null, -1),
     new Definition("1100011111111001", "ENQT",     "EagleSpecial", "noArguments", EagleSpecial.ENQT),
     new Definition("110yy1ii00111000", "ESTA",     null, null, -1),
     new Definition("101yy1ii01111000", "ESTB",     null, null, -1),
     new Definition("110yy11000101000", "FAB",      null, null, -1),
     new Definition("1xxyy00001101000", "FAD",      "EagleFloat", "registerRegister", EagleFloat.FAD),
     new Definition("1iiyy01001101000", "FAMD",     null, null, -1),
     new Definition("1iiyy01000101000", "FAMS",     null, null, -1),
     new Definition("1xxyy00000101000", "FAS",      "EagleFloat", "registerRegister", EagleFloat.FAS),
     new Definition("1101011011101000", "FCLE",     null, null, -1),
     new Definition("1xxyy11100101000", "FCMP",     "EagleFloat", "registerRegister", EagleFloat.FCMP),
     new Definition("1xxyy00111101000", "FDD",      "EagleFloat", "registerRegister", EagleFloat.FDD),
     new Definition("1iiyy01111101000", "FDMD",     null, null, -1),
     new Definition("1iiyy01110101000", "FDMS",     null, null, -1),
     new Definition("1xxyy00110101000", "FDS",      "EagleFloat", "registerRegister", EagleFloat.FDS),
     new Definition("101yy11001101000", "FEXP",     "EagleFloat", "register", EagleFloat.FEXP),
     new Definition("1xxyy10110101000", "FFAS",     null, null, -1),
     new Definition("1iiyy10111101000", "FFMD",     null, null, -1),
     new Definition("111yy11001101000", "FHLV",     "EagleFloat", "register", EagleFloat.FHLV),
     new Definition("110yy11001101000", "FINT",     "EagleFloat", "register", EagleFloat.FINT),
     new Definition("1xxyy10100101000", "FLAS",     null, null, -1),
     new Definition("1iiyy10001101000", "FLDD",     null, null, -1),
     new Definition("1iiyy10000101000", "FLDS",     null, null, -1),
     new Definition("1iiyy10101101000", "FLMD",     null, null, -1),
     new Definition("101ii11011101000", "FLST",     null, null, -1),
     new Definition("1xxyy00101101000", "FMD",      "EagleFloat", "registerRegister", EagleFloat.FMD),
     new Definition("1iiyy01101101000", "FMMD",     null, null, -1),
     new Definition("1iiyy01100101000", "FMMS",     null, null, -1),
     new Definition("1xxyy11101101000", "FMOV",     "EagleFloat", "registerRegister", EagleFloat.FMOV),
     new Definition("1xxyy00100101000", "FMS",      "EagleFloat", "registerRegister", EagleFloat.FMS),
     new Definition("111yy11000101000", "FNEG",     null, null, -1),
     new Definition("100yy11000101000", "FNOM",     null, null, -1),
     new Definition("1000011010101000", "FNS",      null, null, -1),
     new Definition("1110111011101000", "FPOP",     null, null, -1),
     new Definition("1110011011101000", "FPSH",     null, null, -1),
     new Definition("1xxyy10011011000", "FRDS",     "EagleFloat", "registerRegister", EagleFloat.FRDS),
     new Definition("101yy11000101000", "FRH",      "EagleFloat", "register", EagleFloat.FRH),
     new Definition("1000111010101000", "FSA",      null, null, -1),
     new Definition("100yy11001101000", "FSCAL",    "EagleFloat", "register", EagleFloat.FSCAL),
     new Definition("1xxyy00011101000", "FSD",      "EagleFloat", "registerRegister", EagleFloat.FSD),
     new Definition("1001011010101000", "FSEQ",     "EagleFloat", "noArguments", EagleFloat.FSEQ),
     new Definition("1010111010101000", "FSGE",     "EagleFloat", "noArguments", EagleFloat.FSGE),
     new Definition("1011111010101000", "FSGT",     "EagleFloat", "noArguments", EagleFloat.FSGT),
     new Definition("1011011010101000", "FSLE",     "EagleFloat", "noArguments", EagleFloat.FSLE),
     new Definition("1010011010101000", "FSLT",     "EagleFloat", "noArguments", EagleFloat.FSLT),
     new Definition("1iiyy01011101000", "FSMD",     null, null, -1),
     new Definition("1iiyy01010101000", "FSMS*",     null, null, -1),
     new Definition("1100111010101000", "FSND*",     null, null, -1),
     new Definition("1001111010101000", "FSNE",     "EagleFloat", "noArguments", EagleFloat.FSNE),
     new Definition("1111111010101000", "FSNER",    null, null, -1),
     new Definition("1100011010101000", "FSNM",     null, null, -1),
     new Definition("1110011010101000", "FSNO*",     null, null, -1),
     new Definition("1110111010101000", "FSNOD",    null, null, -1),
     new Definition("1101011010101000", "FSNU",     null, null, -1),
     new Definition("1101111010101000", "FSNUD",    null, null, -1),
     new Definition("1111011010101000", "FSNUO",    null, null, -1),
     new Definition("1xxyy00010101000", "FSS",      "EagleFloat", "registerRegister", EagleFloat.FSS),
     new Definition("100ii11011101000", "FSST",     null, null, -1),
     new Definition("1iiyy10011101000", "FSTD",     null, null, -1),
     new Definition("1iiyy10010101000", "FSTS",     null, null, -1),
     new Definition("1100111011101000", "FTD",      "EagleFloat", "noArguments", EagleFloat.FTD),
     new Definition("1100011011101000", "FTE",      "EagleFloat", "noArguments", EagleFloat.FTE),
     new Definition("1010011101111001", "FXTD",     null, null, -1),
     new Definition("1100011101001001", "FXTE",     null, null, -1),
     new Definition("110yy11011111000", "HLV",      null, null, -1),
     new Definition("1nnyy01100001000", "HXL",      null, null, -1),
     new Definition("1nnyy01101001000", "HXR",      null, null, -1),
     new Definition("1xxyy00100001000", "IOR",      null, null, -1),
     new Definition("100yy11111111000", "IORI",     null, null, -1),
     new Definition("1100011111001001", "ISZTS",    "EagleStack", "noArguments", EagleStack.ISZTS),
     new Definition("101ii11011001001", "LCALL",    "EagleStack", "wideIndirectArgument", EagleStack.LCALL),
     new Definition("1000011101011001", "LCPID",    null, null, -1),
     new Definition("110yy11001101001", "LDAFP",    "EagleStack", "register", EagleStack.LDAFP),
     new Definition("110yy11001001001", "LDASB",    "EagleStack", "register", EagleStack.LDASB),
     new Definition("101yy11001101001", "LDASL",    "EagleStack", "register", EagleStack.LDASL),
     new Definition("101yy11001001001", "LDASP",    "EagleStack", "register", EagleStack.LDASP),
     new Definition("100yy11001001001", "LDATS",    "EagleStack", "register", EagleStack.LDATS),
     new Definition("1xxyy10111001000", "LDB",      null, null, -1),
     new Definition("100yy11110101000", "LDI",      null, null, -1),
     new Definition("1100011110101000", "LDIX",     null, null, -1),
     new Definition("1iiyy10100011001", "LDSP",     "EagleGeneral", "registerWideIndirect", EagleGeneral.LDSP),
     new Definition("1iiyy00011011001", "LFAMD",    "EagleFloat", "registerWideIndirect", EagleFloat.LFAMD),
     new Definition("1iiyy00011001001", "LFAMS",    "EagleFloat", "registerWideIndirect", EagleFloat.LFAMS),
     new Definition("1iiyy00111111001", "LFDMD",    null, null, -1),
     new Definition("1iiyy00111101001", "LFDMS",    null, null, -1),
     new Definition("1iiyy01011011001", "LFLDD",    "EagleFloat", "registerWideIndirect", EagleFloat.LFLDD),
     new Definition("1iiyy01011001001", "LFLDS",    "EagleFloat", "registerWideIndirect", EagleFloat.LFLDS),
     new Definition("110ii11011011001", "LFLST",    null, null, -1),
     new Definition("1iiyy00111011001", "LFMMD",    "EagleFloat", "registerWideIndirect", EagleFloat.LFMMD),
     new Definition("1iiyy00111001001", "LFMMS",    "EagleFloat", "registerWideIndirect", EagleFloat.LFMMS),
     new Definition("1iiyy00011111001", "LFSMD*",    null, null, -1),
     new Definition("1iiyy00011101001", "LFSMS*",    null, null, -1),
     new Definition("110ii11011101001", "LFSST*",    null, null, -1),
     new Definition("1iiyy01011111001", "LFSTD",    "EagleFloat", "registerWideIndirect", EagleFloat.LFSTD),
     new Definition("1iiyy01011101001", "LFSTS",    "EagleFloat", "registerWideIndirect", EagleFloat.LFSTS),
     new Definition("101ii11011011001", "LJMP",     "EagleGeneral", "wideIndirect", EagleGeneral.LJMP),
     new Definition("101ii11011101001", "LJSR",     "EagleGeneral", "wideIndirect", EagleGeneral.LJSR),
     new Definition("1iiyy10011001001", "LLDB",     "EagleGeneral", "registerWideByteIndexed", EagleGeneral.LLDB),
     new Definition("1iiyy01111101001", "LLEF",     "EagleGeneral", "registerWideIndirect", EagleGeneral.LLEF),
     new Definition("1iiyy10011101001", "LLEFB",    "EagleGeneral", "registerWideIndirect", EagleGeneral.LLEFB),
     new Definition("1000011111001001", "LMRF",     null, null, -1),
     new Definition("1iiyy01000011000", "LNADD",    "EagleCompute", "registerWideIndirect", EagleCompute.LNADD),
     new Definition("1nnii11000011000", "LNADI",    "EagleCompute", "tinyImmediateWordIndirect", EagleCompute.LNADI),
     new Definition("1iiyy01011011000", "LNDIV",    null, null, -1),
     new Definition("1yyii11010011000", "LNDO",     "EagleGeneral", "wideIndirectArgument", EagleGeneral.LNDO),
     new Definition("100ii11011011001", "LNDSZ",    null, null, -1),
     new Definition("100ii11011001001", "LNISZ",    null, null, -1),
     new Definition("1iiyy01111001001", "LNLDA",    "EagleGeneral", "registerWideIndirect", EagleGeneral.LNLDA),
     new Definition("1iiyy01010011000", "LNMUL",    "EagleCompute", "registerWideIndirect", EagleCompute.LNMUL),
     new Definition("1nnii11001011000", "LNSBI",    "EagleCompute", "tinyImmediateWordIndirect", EagleCompute.LNSBI),
     new Definition("1iiyy01111011001", "LNSTA",    "EagleGeneral", "registerWideIndirect", EagleGeneral.LNSTA),
     new Definition("1iiyy01001011000", "LNSUB",    "EagleCompute", "registerWideIndirect", EagleCompute.LNSUB),
     new Definition("1xxyy10100001000", "LOB",      null, null, -1),
     new Definition("101ii11011111001", "LPEF",     "EagleStack", "wideIndirect", EagleStack.LPEF),
     new Definition("110ii11011111001", "LPEFB",    "EagleStack", "wideIndirect", EagleStack.LPEFB),
     new Definition("1000011111101001", "LPHY",     null, null, -1),
     new Definition("110ii11011001001", "LPSHJ",    "EagleStack", "wideIndirect", EagleStack.LPSHJ),
     new Definition("1010011110011001", "LPSR",     "EagleGeneral", "noArguments", EagleGeneral.LPSR),
     new Definition("1110011100111001", "LPTE",     null, null, -1),
     new Definition("1xxyy10101001000", "LRB",      null, null, -1),
     new Definition("1100011110111001", "LSBRA",    null, null, -1),
     new Definition("1110011110001001", "LSBRS",    null, null, -1),
     new Definition("1xxyy01010001000", "LSH",      null, null, -1),
     new Definition("1111111110101000", "LSN",      null, null, -1),
     new Definition("1iiyy10011011001", "LSTB",     "EagleGeneral", "registerWideByteIndexed", EagleGeneral.LSTB),
     new Definition("1iiyy01100011000", "LWADD",    "EagleCompute", "registerWideIndirect", EagleCompute.LWADD),
     new Definition("1nnii11100011000", "LWADI",    null, null, -1),
     new Definition("1iiyy01111011000", "LWDIV",    null, null, -1),
     new Definition("1yyii11110011000", "LWDO",     "EagleGeneral", "wideIndirectArgument", EagleGeneral.LWDO),
     new Definition("100ii11011111001", "LWDSZ",    null, null, -1),
     new Definition("100ii11011101001", "LWISZ",    null, null, -1),
     new Definition("1iiyy01111111001", "LWLDA",    "EagleGeneral", "registerWideIndirect", EagleGeneral.LWLDA),
     new Definition("1iiyy01110011000", "LWMUL",    "EagleCompute", "registerWideIndirect", EagleCompute.LWMUL),
     new Definition("1nnii11101011000", "LWSBI",    null, null, -1),
     new Definition("1iiyy10011111001", "LWSTA",    "EagleGeneral", "registerWideIndirect", EagleGeneral.LWSTA),
     new Definition("1iiyy01101011000", "LWSUB",    "EagleCompute", "registerWideIndirect", EagleCompute.LWSUB),
     new Definition("100yy11011111000", "MSP",      null, null, -1),
     new Definition("1100011111001000", "MUL",      null, null, -1),
     new Definition("1100111111001000", "MULS",     null, null, -1),
     new Definition("1xxyy00001001001", "NADD",     "EagleCompute", "registerRegister", EagleCompute.NADD),
     new Definition("110yy11000111001", "NADDI",    "EagleCompute", "registerWordImmediate", EagleCompute.NADDI),
     new Definition("1nnyy10110011001", "NADI",     "EagleCompute", "tinyImmediateRegister", EagleCompute.NADI),
     new Definition("1xxyy00001111001", "NDIV",     null, null, -1),
     new Definition("110yy11000101001", "NLDAI",    "EagleCompute", "wordImmediateRegister", EagleCompute.NLDAI),
     new Definition("1xxyy00001101001", "NMUL",     "EagleCompute", "registerRegister", EagleCompute.NMUL),
     new Definition("1xxyy10100001001", "NNEG",     "EagleCompute", "registerRegister", EagleCompute.NNEG),
     new Definition("111yy11000001001", "NSALA",    null, null, -1),
     new Definition("111yy11000011001", "NSALM",    null, null, -1),
     new Definition("111yy11000101001", "NSANA",    "EagleCompute", "registerWordImmediate", EagleCompute.NSANA),
     new Definition("111yy11000111001", "NSANM",    null, null, -1),
     new Definition("1nnyy10110101001", "NSBI",     "EagleCompute", "tinyImmediateRegister", EagleCompute.NSBI),
     new Definition("1xxyy00001011001", "NSUB",     "EagleCompute", "registerRegister", EagleCompute.NSUB),
     new Definition("1110011110111001", "ORFB",     null, null, -1),
     new Definition("1110011110011001", "PATU*",     null, null, -1),
     new Definition("1000011101001001", "PBX*",      null, null, -1),
     new Definition("1xxyy10111011001", "PIO",      null, null, -1),
     new Definition("1xxyy11010001000", "POP",      null, null, -1),
     new Definition("1000111111001000", "POPB",     null, null, -1),
     new Definition("1001111111001000", "POPJ",     null, null, -1),
     new Definition("1xxyy11001001000", "PSH",      null, null, -1),
     new Definition("100001ii10111000", "PSHJ",     null, null, -1),
     new Definition("1000011111001000", "PSHR",     null, null, -1),
     new Definition("1110011110101001", "RRFB",     null, null, -1),
     new Definition("1110111111001000", "RSTR",     null, null, -1),
     new Definition("1010111111001000", "RTN",      null, null, -1),
     new Definition("1110011111001000", "SAVE",     null, null, -1),
     new Definition("1010011111001000", "SAVZ",     null, null, -1),
     new Definition("1nnyy00001001000", "SBI",      null, null, -1),
     new Definition("1xxyy01101001001", "SEX",      "EagleCompute", "registerRegister", EagleCompute.SEX),
     new Definition("1xxyy01001001000", "SGE",      null, null, -1),
     new Definition("1xxyy01000001000", "SGT",      null, null, -1),
     new Definition("1000011111011001", "SMRF",     null, null, -1),
     new Definition("1xxyy10111111000", "SNB",      null, null, -1),
     new Definition("1010011110111001", "SNOVR",    null, null, -1),
     new Definition("1010011110101001", "SPSR",     null, null, -1),
     new Definition("1110011100101001", "SPTE",     null, null, -1),
     new Definition("1110011111011001", "SSPT",     null, null, -1),
     new Definition("110yy11001111001", "STAFP",    "EagleStack", "register", EagleStack.STAFP),
     new Definition("110yy11001011001", "STASB",    "EagleStack", "register", EagleStack.STASB),
     new Definition("101yy11001111001", "STASL",    "EagleStack", "register", EagleStack.STASL),
     new Definition("101yy11001011001", "STASP",    "EagleStack", "register", EagleStack.STASP),
     new Definition("100yy11001011001", "STATS",    "EagleStack", "register", EagleStack.STATS),
     new Definition("1xxyy11000001000", "STB",      null, null, -1),
     new Definition("101yy11110101000", "STI",      null, null, -1),
     new Definition("1100111110101000", "STIX",     null, null, -1),
     new Definition("1xxyy10010001000", "SZB",      null, null, -1),
     new Definition("1xxyy10011001000", "SZBO",     null, null, -1),
     new Definition("1100011110011001", "VBP",      null, null, -1),
     new Definition("1100011110101001", "VWP",      null, null, -1),
     new Definition("1xxyy01001001001", "WADC",     "EagleCompute", "registerRegister", EagleCompute.WADC),
     new Definition("1xxyy00101001001", "WADD",     "EagleCompute", "registerRegister", EagleCompute.WADD),
     new Definition("100yy11010001001", "WADDI",    "EagleCompute", "registerWideImmediate", EagleCompute.WADDI),
     new Definition("1nnyy10010111001", "WADI",     "EagleCompute", "tinyImmediateRegister", EagleCompute.WADI),
     new Definition("1xxyy10101001001", "WANC",     null, null, -1),
     new Definition("1xxyy10001001001", "WAND",     "EagleCompute", "registerRegister", EagleCompute.WAND),
     new Definition("100yy11010011001", "WANDI",    "EagleCompute", "registerWideImmediate", EagleCompute.WANDI),
     new Definition("1xxyy01001111001", "WASH",     null, null, -1),
     new Definition("110yy11010101001", "WASHI",    null, null, -1),
     new Definition("1110011101001001", "WBLM",     "EagleSpecial", "noArguments", EagleSpecial.WBLM),
     new Definition("1dddd0dddd111000", "WBR",      "EagleGeneral", "shortDisplacement", EagleGeneral.WBR),
     new Definition("1100011100011001", "QSEARCH*", "EagleSpecial", "wordImmediate", EagleSpecial.QSEARCH),    // forward or backward is determined by next word
     new Definition("1xxyy01010011001", "WBTO",     "EagleCompute", "registerRegister", EagleCompute.WBTO),
     new Definition("1xxyy01010101001", "WBTZ",     "EagleCompute", "registerRegister", EagleCompute.WBTZ),
     new Definition("1xxyy10101101001", "WCLM",     "EagleGeneral", "registerRegister", EagleGeneral.WCLM),
     new Definition("1010011101011001", "WCMP",     "EagleSpecial", "noArguments", EagleSpecial.WCMP),
     new Definition("1010011101001001", "WCMT",     null, null, -1),
     new Definition("1000011101111001", "WCMV",     "EagleSpecial", "noArguments", EagleSpecial.WCMV),
     new Definition("1xxyy10010001001", "WCOB",     null, null, -1),
     new Definition("1xxyy10001011001", "WCOM",     "EagleCompute", "registerRegister", EagleCompute.WCOM),
     new Definition("1110011100001001", "WCST",     "EagleSpecial", "noArguments", EagleSpecial.WCST),
     new Definition("1000011101101001", "WCTR*",     null, null, -1),
     new Definition("1000011100011001", "W_DEC",    null, null, -1),
     new Definition("1xxyy00101111001", "WDIV",     "EagleCompute", "registerRegister", EagleCompute.WDIV),
     new Definition("1110011101101001", "WDIVS",    "EagleCompute", "noArguments", EagleCompute.WDIVS),
     new Definition("1000011111111001", "WDPOP",    null, null, -1),
     new Definition("1010011101101001", "WEDIT",    null, null, -1),
     new Definition("1xxyy10010011001", "WFFAD",    "EagleFloat", "registerRegister", EagleFloat.WFFAD),
     new Definition("1xxyy10010101001", "WFLAD",    "EagleFloat", "registerRegister", EagleFloat.WFLAD),
     new Definition("1010011110001001", "WFPOP",    "EagleStack", "noArguments", EagleStack.WFPOP),
     new Definition("1000011110111001", "WFPSH",    "EagleStack", "noArguments", EagleStack.WFPSH),
     new Definition("1000111001111001", "FP_INTR",  null, null, -1),
     new Definition("1000111001101001", "GRAPHICS", null, null, -1),
     new Definition("111yy11001011001", "WHLV",     "EagleCompute", "register", EagleCompute.WHLV),
     new Definition("1xxyy01001011001", "WINC",     "EagleCompute", "registerRegister", EagleCompute.WINC),
     new Definition("1xxyy10001101001", "WIOR",     "EagleCompute", "registerRegister", EagleCompute.WIOR),
     new Definition("100yy11010101001", "WIORI",    "EagleCompute", "registerWideImmediate", EagleCompute.WIORI),
     new Definition("110yy11010001001", "WLDAI",    "EagleCompute", "wideImmediate", EagleCompute.WLDAI),
     new Definition("1xxyy10100101001", "WLDB",     "EagleGeneral", "registerRegister", EagleGeneral.WLDB),
     new Definition("111yy11001111001", "WLDI",     null, null, -1),
     new Definition("1100011101011001", "WLDIX",    null, null, -1),
     new Definition("1010011111111001", "WLMP",     null, null, -1),
     new Definition("1xxyy01110101001", "WLOB",     "EagleCompute", "registerRegister", EagleCompute.WLOB),
     new Definition("1xxyy01110111001", "WLRB",     null, null, -1),
     new Definition("1xxyy10101011001", "WLSH",     "EagleCompute", "registerRegister", EagleCompute.WLSH),
     new Definition("111yy11011011001", "WLSHI",    "EagleCompute", "registerWordImmediate", EagleCompute.WLSHI),
     new Definition("1nnyy10110111001", "WLSI",     "EagleCompute", "tinyImmediateRegister", EagleCompute.WLSI),
     new Definition("1100011101111001", "WLSN",     null, null, -1),
     new Definition("1110011100011001", "WMESS",    "EagleSpecial", "noArguments", EagleSpecial.WMESS),
     new Definition("1xxyy01101111001", "WMOV",     "EagleCompute", "registerRegister", EagleCompute.WMOV),
     new Definition("111yy11010011001", "WMOVR",    "EagleCompute", "register", EagleCompute.WMOVR),
     new Definition("111yy11001001001", "WMSP",     "EagleStack", "register", EagleStack.WMSP),
     new Definition("1xxyy00101101001", "WMUL",     "EagleCompute", "registerRegister", EagleCompute.WMUL),
     new Definition("1110011101011001", "WMULS",    null, null, -1),
     new Definition("111yy11011111001", "WNADI",    "EagleCompute", "registerWordImmediate", EagleCompute.WNADI),
     new Definition("1xxyy01001101001", "WNEG",     "EagleCompute", "registerRegister", EagleCompute.WNEG),
     new Definition("1xxyy00010001001", "WPOP",     "EagleStack", "registerRegister", EagleStack.WPOP),
     new Definition("1110011101111001", "WPOPB",    "EagleStack", "noArguments", EagleStack.WPOPB),
     new Definition("1000011110001001", "WPOPJ",    "EagleStack", "noArguments", EagleStack.WPOPJ),
     new Definition("1xxyy10101111001", "WPSH",     "EagleStack", "registerRegister", EagleStack.WPSH),
     new Definition("1000011110011001", "WRSTR",    null, null, -1),
     new Definition("1000011110101001", "WRTN",     "EagleStack", "noArguments", EagleStack.WRTN),
     new Definition("101yy11010011001", "WSALA",    null, null, -1),
     new Definition("101yy11010111001", "WSALM",    null, null, -1),
     new Definition("101yy11010001001", "WSANA",    "EagleCompute", "registerWideImmediate", EagleCompute.WSANA),
     new Definition("101yy11010101001", "WSANM",    null, null, -1),
     new Definition("1010011100101001", "WSAVR",    "EagleStack", "wordImmediate", EagleStack.WSAVR),
     new Definition("1010011100111001", "WSAVS",    "EagleStack", "wordImmediate", EagleStack.WSAVS),
     new Definition("1nnyy10110001001", "WSBI",     "EagleCompute", "tinyImmediateRegister", EagleCompute.WSBI),
     new Definition("1xxyy00010111001", "WSEQ",     "EagleCompute", "registerRegister", EagleCompute.WSEQ),
     new Definition("111yy11011001001", "WSEQI",    "EagleCompute", "registerWordImmediate", EagleCompute.WSEQI),
     new Definition("1xxyy00110011001", "WSGE",     "EagleCompute", "registerRegister", EagleCompute.WSGE),
     new Definition("1xxyy00110111001", "WSGT",     "EagleCompute", "registerRegister", EagleCompute.WSGT),
     new Definition("111yy11010001001", "WSGTI",    "EagleCompute", "registerWordImmediate", EagleCompute.WSGTI),
     new Definition("1bbb111101bb1001", "WSKBO",    "EagleCompute", "bitPosition", EagleCompute.WSKBO),
     new Definition("1bbb111110bb1001", "WSKBZ",    "EagleCompute", "bitPosition", EagleCompute.WSKBZ),
     new Definition("1xxyy00110101001", "WSLE",     "EagleCompute", "registerRegister", EagleCompute.WSLE),
     new Definition("111yy11010101001", "WSLEI",    "EagleCompute", "registerWordImmediate", EagleCompute.WSLEI),
     new Definition("1xxyy01010001001", "WSLT",     "EagleCompute", "registerRegister", EagleCompute.WSLT),
     new Definition("1xxyy01110001001", "WSNB",     null, null, -1),
     new Definition("1xxyy00110001001", "WSNE",     "EagleCompute", "registerRegister", EagleCompute.WSNE),
     new Definition("111yy11011101001", "WSNEI",    "EagleCompute", "registerWordImmediate", EagleCompute.WSNEI),
     new Definition("1000011100101001", "WSSVR",    "EagleStack", "wordImmediate", EagleStack.WSSVR),
     new Definition("1000011100111001", "WSSVS",    "EagleStack", "wordImmediate", EagleStack.WSSVS),
     new Definition("1xxyy10100111001", "WSTB",     "EagleGeneral", "registerRegister", EagleGeneral.WSTB),
     new Definition("111yy11010111001", "WSTI",     null, null, -1),
     new Definition("1100011101101001", "WSTIX",    null, null, -1),
     new Definition("1xxyy00101011001", "WSUB",     "EagleCompute", "registerRegister", EagleCompute.WSUB),
     new Definition("1xxyy01010111001", "WSZB",     "EagleCompute", "registerRegister", EagleCompute.WSZB),
     new Definition("1xxyy01110011001", "WSZBO",    "EagleCompute", "registerRegister", EagleCompute.WSZBO),
     new Definition("110yy11010011001", "WUGTI",    "EagleCompute", "registerWideImmediate", EagleCompute.WUGTI),
     new Definition("110yy11010111001", "WULEI",    "EagleCompute", "registerWideImmediate", EagleCompute.WULEI),
     new Definition("1xxyy00010011001", "WUSGE",    "EagleCompute", "registerRegister", EagleCompute.WUSGE),
     new Definition("1xxyy00010101001", "WUSGT",    "EagleCompute", "registerRegister", EagleCompute.WUSGT),
     new Definition("1xxyy01101101001", "WXCH",     "EagleCompute", "registerRegister", EagleCompute.WXCH),
     new Definition("1010011100001001", "WXOP",     null, null, -1),
     new Definition("1xxyy10001111001", "WXOR",     "EagleCompute", "registerRegister", EagleCompute.WXOR),
     new Definition("100yy11010111001", "WXORI",    "EagleCompute", "registerWideImmediate", EagleCompute.WXORI),
     new Definition("100ii11000001001", "XCALL",    "EagleStack", "wordIndirectArgument", EagleStack.XCALL),
     new Definition("1xxyy00111001000", "XCH",      null, null, -1),
     new Definition("101yy11011111000", "XCT*",      null, null, -1),
     new Definition("1iiyy00000011001", "XFAMD",    "EagleFloat", "registerWordIndirect", EagleFloat.XFAMD),
     new Definition("1iiyy00000001001", "XFAMS",    "EagleFloat", "registerWordIndirect", EagleFloat.XFAMS),
     new Definition("1iiyy00100111001", "XFDMD",    null, null, -1),
     new Definition("1iiyy00100101001", "XFDMS",    null, null, -1),
     new Definition("1iiyy01000011001", "XFLDD",    "EagleFloat", "registerWordIndirect", EagleFloat.XFLDD),
     new Definition("1iiyy01000001001", "XFLDS",    "EagleFloat", "registerWordIndirect", EagleFloat.XFLDS),
     new Definition("1iiyy00000111001", "XFMMD",    "EagleFloat", "registerWordIndirect", EagleFloat.XFMMD),
     new Definition("1iiyy00000101001", "XFMMS",    "EagleFloat", "registerWordIndirect", EagleFloat.XFMMS),
     new Definition("1iiyy00100011001", "XFSMD*",    null, null, -1),
     new Definition("1iiyy00100001001", "XFSMS",    null, null, -1),
     new Definition("1iiyy01000111001", "XFSTD",    "EagleFloat", "registerWordIndirect", EagleFloat.XFSTD),
     new Definition("1iiyy01000101001", "XFSTS",    "EagleFloat", "registerWordIndirect", EagleFloat.XFSTS),
     new Definition("110ii11000001001", "XJMP",     "EagleGeneral", "wordIndirect", EagleGeneral.XJMP),
     new Definition("110ii11000011001", "XJSR",     "EagleGeneral", "wordIndirect", EagleGeneral.XJSR),
     new Definition("1iiyy10000011001", "XLDB",     "EagleGeneral", "registerWordByteIndexed", EagleGeneral.XLDB),
     new Definition("1iiyy10000001001", "XLEF",     "EagleGeneral", "registerWordIndirect", EagleGeneral.XLEF),
     new Definition("1iiyy10000111001", "XLEFB",    "EagleGeneral", "registerWordByteIndexed", EagleGeneral.XLEFB),
     new Definition("1iiyy00000011000", "XNADD",    "EagleCompute", "registerWordIndirect", EagleCompute.XNADD),
     new Definition("1nnii10000011000", "XNADI",    "EagleCompute", "tinyImmediateWordIndirect", EagleCompute.XNADI),
     new Definition("1iiyy00011011000", "XNDIV",    null, null, -1),
     new Definition("1yyii10010011000", "XNDO",     "EagleGeneral", "wordIndirectArgument", EagleGeneral.XNDO),
     new Definition("101ii11000001001", "XNDSZ",    "EagleCompute", "wordIndirect", EagleCompute.XNDSZ),
     new Definition("100ii11000111001", "XNISZ",    "EagleCompute", "wordIndirect", EagleCompute.XNISZ),
     new Definition("1iiyy01100101001", "XNLDA",    "EagleGeneral", "registerWordIndirect", EagleGeneral.XNLDA),
     new Definition("1iiyy00010011000", "XNMUL",    "EagleCompute", "registerWordIndirect", EagleCompute.XNMUL),
     new Definition("1nnii10001011000", "XNSBI",    "EagleCompute", "tinyImmediateWordIndirect", EagleCompute.XNSBI),
     new Definition("1iiyy01100111001", "XNSTA",    "EagleGeneral", "registerWordIndirect", EagleGeneral.XNSTA),
     new Definition("1iiyy00001011000", "XNSUB",    "EagleCompute", "registerWordIndirect", EagleCompute.XNSUB),
     new Definition("1xxyy11011001000", "XOP0",     null, null, -1),
     new Definition("1xxyy00101001000", "XOR",      null, null, -1),
     new Definition("101yy11111111000", "XORI",     null, null, -1),
     new Definition("100ii11000101001", "XPEF",     "EagleStack", "wordIndirect", EagleStack.XPEF),
     new Definition("101ii11000101001", "XPEFB",    "EagleStack", "wordIndirect", EagleStack.XPEFB),
     new Definition("100ii11000011001", "XPSHJ",    "EagleStack", "wordIndirect", EagleStack.XPSHJ),
     new Definition("1iiyy10000101001", "XSTB",     "EagleGeneral", "registerWordByteIndexed", EagleGeneral.XSTB),
     new Definition("1100011100001001", "XVCT",     null, null, -1),
     new Definition("1iiyy00100011000", "XWADD",    "EagleCompute", "registerWordIndirect", EagleCompute.XWADD),
     new Definition("1nnii10100011000", "XWADI",    "EagleCompute", "tinyImmediateWordIndirect", EagleCompute.XWADI),
     new Definition("1iiyy00111011000", "XWDIV",    null, null, -1),
     new Definition("1yyii10110011000", "XWDO",     "EagleGeneral", "wordIndirectArgument", EagleGeneral.XWDO),
     new Definition("101ii11000111001", "XWDSZ",    null, null, -1),
     new Definition("101ii11000011001", "XWISZ",    "EagleCompute", "wordIndirect", EagleCompute.XWISZ),
     new Definition("1iiyy01100001001", "XWLDA",    "EagleGeneral", "registerWordIndirect", EagleGeneral.XWLDA),
     new Definition("1iiyy00110011000", "XWMUL",    "EagleCompute", "registerWordIndirect", EagleCompute.XWMUL),
     new Definition("1nnii10101011000", "XWSBI",    "EagleCompute", "tinyImmediateWordIndirect", EagleCompute.XWSBI),
     new Definition("1iiyy01100011001", "XWSTA",    "EagleGeneral", "registerWordIndirect", EagleGeneral.XWSTA),
     new Definition("1iiyy00101011000", "XWSUB",    "EagleCompute", "registerWordIndirect", EagleCompute.XWSUB),
     new Definition("1xxyy01101011001", "ZEX",      "EagleCompute", "registerRegister", EagleCompute.ZEX)
   };

   static public int maskForMatch(String match) {
    int index, mask;

    if(match.length()!=16)
     throw new RuntimeException("Invalid instruction mask: " + match);
    mask=0;
    for(index=0;index<16;index++) {
     mask=mask*2;
     if(match.charAt(index)=='0' || match.charAt(index)=='1')
      mask++;
    }
    return mask;
   }

   static public int valueForMatch(String match) {
    int index, value;

    if(match.length()!=16)
     throw new RuntimeException("Invalid instruction mask: " + match);
    value=0;
    for(index=0;index<16;index++) {
     value=value*2;
     if(match.charAt(index)=='1')
      value++;
    }
    return value;
   }

   static public Instruction instantiate(String className) {
    try {
     Object instance=Class.forName("hw." + className).getDeclaredConstructor().newInstance();

     if(!(instance instanceof Instruction))
      throw new RuntimeException("Class '" + className + "' must be a subclass of Instruction");
     return (Instruction)instance;
    }
    catch(ClassNotFoundException exception) {
     throw new RuntimeException("Instruction class '" + className + "' not found");
    }
    catch(ReflectiveOperationException exception) {
     throw new RuntimeException("Error instantiating class '" + className + "' - " + exception.getMessage());
    }
   }

   static public Instruction findOpcode(Definition[] table, int opcode) {
    Instruction instruction=null;

    for(int index=0;index<table.length;index++) {
     int mask=maskForMatch(table[index].match);
     int value=valueForMatch(table[index].match);

     if((opcode & mask)==value) {
      if(instruction!=null)
       throw new RuntimeException("Instruction is not unique for opcode " + String.format("%04X", opcode));
      if(table[index].instructionClass==null)
       instruction=new Instruction();
      else
       instruction=instantiate(table[index].instructionClass);
      instruction.setup(opcode, table[index].name, table[index].instructionFormat, table[index].operator);
     }
    }
    return instruction;
   }

   static public Instruction decode(boolean lefMode, int opcode) {
    int bottom;

    if(opcode<0x6000)
     return findOpcode(novaGeneralOpcodes, opcode);
    if(opcode<0x8000)
     return findOpcode(lefMode ? novaLEFOpcodes : novaIOOpcodes, opcode);
    bottom=opcode & 0x0F;
    return findOpcode((bottom==8 || bottom==9) ? eclipseMVOpcodes : novaComputeOpcodes, opcode);
   }

   static public Instruction[] novaGeneralTable() {
    Instruction[] instructions=new Instruction[12];
    int           index=0;

    for(int opcode=0x0000;opcode<0x6000;opcode+=0x800)
     instructions[index++]=decode(false, opcode);
    return instructions;
   }

   static public Instruction[] novaIOTable() {
    Instruction[] instructions=new Instruction[32];
    int           index=0;

    for(int opcode=0x6000;opcode<0x8000;opcode+=0x100)
     instructions[index++]=decode(true, opcode);
    return instructions;
   }

   static public Instruction[] novaComputeTable() {
    Instruction[] instructions=new Instruction[8];
    int           index=0;

    for(int opcode=0x8000;opcode<0x8800;opcode+=0x100)
     instructions[index++]=decode(false, opcode);
    return instructions;
   }

   static public Instruction[] eclipseMVTable() {
    Instruction[] instructions=new Instruction[4096];
    int           index=0;

    for(int opcode=0x8000;opcode<0x10000;opcode+=16) {
     instructions[index++]=decode(false, opcode+8);
     instructions[index++]=decode(false, opcode+9);
    }
    return instructions;
   }
}
