#include <stdio.h>
#include <string.h>
#include "flux-isa-v3.h"

static int tests_run = 0;
static int tests_pass = 0;

#define ASSERT(cond, msg) do { tests_run++; if (cond) { tests_pass++; } else { printf("FAIL: %s (line %d)\n", msg, __LINE__); } } while(0)

void test_length_1byte() {
    uint8_t nop = 0x00;
    ASSERT(isa_v3_length(nop) == 1, "NOP is 1 byte");
    uint8_t halt = 0x03;
    ASSERT(isa_v3_length(halt) == 1, "HALT variant is 1 byte");
}

void test_length_2byte() {
    uint8_t push = isa_v3_make_op(OP_CAT_STACK, 0x00);
    ASSERT(isa_v3_length(push) == 2, "Stack op is 2 bytes");
    uint8_t conf_add = isa_v3_make_op(OP_CAT_CONF, 0x00);
    ASSERT(isa_v3_length(conf_add) == 2, "CONF op is 2 bytes");
}

void test_length_3byte() {
    uint8_t jmp = isa_v3_make_op(OP_CAT_CTRL, 0x00);
    ASSERT(isa_v3_length(jmp) == 3, "CTRL op is 3 bytes");
    uint8_t inst = isa_v3_make_op(OP_CAT_INSTINCT, 0x00);
    ASSERT(isa_v3_length(inst) == 3, "INSTINCT op is 3 bytes");
    uint8_t nrg = isa_v3_make_op(OP_CAT_ENERGY, 0x00);
    ASSERT(isa_v3_length(nrg) == 3, "ENERGY op is 3 bytes");
}

void test_decode_1byte() {
    isa_v3_insn_t insn;
    uint8_t buf[] = {0x01}; /* NOP variant */
    int len = isa_v3_decode(buf, 1, &insn);
    ASSERT(len == 1, "decode 1-byte returns 1");
    ASSERT(insn.opcode == 0x01, "opcode matches");
    ASSERT(insn.cat == 0x0, "category NOP");
}

void test_decode_2byte() {
    isa_v3_insn_t insn;
    uint8_t buf[] = {0x20, 0x05}; /* ALU op, dest=reg5 */
    int len = isa_v3_decode(buf, 2, &insn);
    ASSERT(len == 2, "decode 2-byte returns 2");
    ASSERT(insn.rd == 5, "dest register 5");
    ASSERT(insn.rs == 0, "no source for 2-byte");
}

void test_decode_3byte() {
    isa_v3_insn_t insn;
    uint8_t buf[] = {0x60, 0x03, 0x07}; /* CTRL, rd=3, rs=7 */
    int len = isa_v3_decode(buf, 3, &insn);
    ASSERT(len == 3, "decode 3-byte returns 3");
    ASSERT(insn.rd == 3, "dest 3");
    ASSERT(insn.rs == 7, "source 7");
}

void test_decode_conf_fused() {
    isa_v3_insn_t insn;
    uint8_t buf[] = {0x4D, 0x02}; /* CONF op, sub_op bits = (0x4D>>2)&3=1, conf=0x4D&3=1 */
    int len = isa_v3_decode(buf, 2, &insn);
    ASSERT(len == 2, "CONF decode length");
    ASSERT(insn.conf == 1, "confidence bits = 1");
    ASSERT(insn.cat == 0x4, "CONF category");
}

void test_encode_1byte() {
    uint8_t buf[4] = {0};
    int n = isa_v3_encode(0x02, 0, 0, buf, 4);
    ASSERT(n == 1, "encode 1-byte returns 1");
    ASSERT(buf[0] == 0x02, "encoded byte matches");
}

void test_encode_2byte() {
    uint8_t buf[4] = {0};
    int n = isa_v3_encode(0x20, 0x0A, 0, buf, 4);
    ASSERT(n == 2, "encode 2-byte returns 2");
    ASSERT(buf[0] == 0x20, "byte 0 correct");
    ASSERT(buf[1] == 0x0A, "byte 1 correct");
}

void test_encode_3byte() {
    uint8_t buf[4] = {0};
    int n = isa_v3_encode(0xB0, 0x05, 0x2A, buf, 4);
    ASSERT(n == 3, "encode 3-byte returns 3");
    ASSERT(buf[0] == 0xB0, "byte 0");
    ASSERT(buf[1] == 0x05, "byte 1");
    ASSERT(buf[2] == 0x2A, "byte 2");
}

void test_roundtrip() {
    uint8_t encoded[4];
    isa_v3_insn_t decoded;
    /* Test 3-byte roundtrip */
    isa_v3_encode(0x70, 0x10, 0x20, encoded, 4);
    int len = isa_v3_decode(encoded, 4, &decoded);
    ASSERT(len == 3, "roundtrip length");
    ASSERT(decoded.opcode == 0x70, "roundtrip opcode");
    ASSERT(decoded.rd == 0x10, "roundtrip rd");
    ASSERT(decoded.rs == 0x20, "roundtrip rs");
}

void test_make_op() {
    uint8_t op = isa_v3_make_op(OP_CAT_ALU, 0x08);
    ASSERT(op == 0x28, "make_op ALU sub=8");
}

void test_make_conf_op() {
    uint8_t op = isa_v3_make_conf_op(0x5, 0x04, 0x02);
    /* cat=5<<4=0x50, sub=0x04 (bits 3:2), conf=0x02 (bits 1:0) => 0x56 */
    ASSERT(op == 0x56, "make_conf_op: cat=5, sub=0x4 with conf=2");
    ASSERT((op & 0x03) == 0x02, "conf bits correct");
}

void test_sub_op() {
    uint8_t normal = 0x27; /* cat=2, sub=7 */
    ASSERT(isa_v3_sub_op(normal, 0x2) == 7, "normal sub_op");
    uint8_t conf = 0x54; /* cat=5, sub=(0x54>>2)&3=1, conf=0x54&3=0 */
    ASSERT(isa_v3_sub_op(conf, 0x5) == 1, "conf sub_op skips conf bits");
}

void test_reg_imm_helpers() {
    ASSERT(ISA_V3_IS_REG(5), "5 is a register");
    ASSERT(!ISA_V3_IS_REG(70), "70 is immediate");
    ASSERT(ISA_V3_IS_IMM(70), "70 is immediate");
    ASSERT(ISA_V3_IMM_VAL(70) == 6, "imm val 70-64=6");
    ASSERT(ISA_V3_MAKE_IMM(10) == 74, "make imm 10=74");
    ASSERT(ISA_V3_MAKE_REG(5) == 5, "make reg 5");
}

void test_decode_short_buffer() {
    isa_v3_insn_t insn;
    uint8_t buf[] = {0x20}; /* 2-byte but only 1 given */
    int len = isa_v3_decode(buf, 1, &insn);
    ASSERT(len == 0, "short buffer returns 0");
}

void test_decode_empty() {
    isa_v3_insn_t insn;
    int len = isa_v3_decode(NULL, 0, &insn);
    ASSERT(len == 0, "empty buffer returns 0");
}

void test_encode_short_buffer() {
    uint8_t buf[1];
    int n = isa_v3_encode(0x60, 0, 0, buf, 1);
    ASSERT(n == 0, "encode 3-byte in 1-byte buffer fails");
}

void test_multi_instruction_stream() {
    /* Decode a stream of mixed-length instructions */
    uint8_t stream[] = {0x00, 0x20, 0x05, 0xB0, 0x03, 0x07, 0x02};
    isa_v3_insn_t insn;
    size_t pos = 0;
    
    /* 0x00 -> 1 byte NOP */
    pos += isa_v3_decode(stream + pos, 7 - pos, &insn);
    ASSERT(pos == 1, "stream: NOP at pos 1");
    
    /* 0x20,0x05 -> 2 byte ALU */
    pos += isa_v3_decode(stream + pos, 7 - pos, &insn);
    ASSERT(pos == 3, "stream: ALU at pos 3");
    
    /* 0xB0,0x03,0x07 -> 3 byte ENERGY */
    pos += isa_v3_decode(stream + pos, 7 - pos, &insn);
    ASSERT(pos == 6, "stream: ENERGY at pos 6");
    ASSERT(insn.rd == 3 && insn.rs == 7, "stream: ENERGY operands");
    
    /* 0x02 -> 1 byte */
    pos += isa_v3_decode(stream + pos, 7 - pos, &insn);
    ASSERT(pos == 7, "stream: end at pos 7");
}

void test_all_categories_have_length() {
    /* Every category should produce a valid length */
    for (int cat = 0; cat < 16; cat++) {
        uint8_t op = (uint8_t)(cat << 4);
        int len = isa_v3_length(op);
        ASSERT(len >= 1 && len <= 3, "all categories have valid length");
    }
}

void test_conf_fusion_preserves_confidence() {
    for (int c = 0; c < 4; c++) {
        uint8_t op = isa_v3_make_conf_op(0x5, 0x0C, (uint8_t)c);
        ASSERT((op & 0x03) == c, "conf fusion preserves all conf values");
    }
}

int main() {
    test_length_1byte();
    test_length_2byte();
    test_length_3byte();
    test_decode_1byte();
    test_decode_2byte();
    test_decode_3byte();
    test_decode_conf_fused();
    test_encode_1byte();
    test_encode_2byte();
    test_encode_3byte();
    test_roundtrip();
    test_make_op();
    test_make_conf_op();
    test_sub_op();
    test_reg_imm_helpers();
    test_decode_short_buffer();
    test_decode_empty();
    test_encode_short_buffer();
    test_multi_instruction_stream();
    test_all_categories_have_length();
    test_conf_fusion_preserves_confidence();
    
    printf("\n%d/%d tests passed\n", tests_pass, tests_run);
    return (tests_pass == tests_run) ? 0 : 1;
}
