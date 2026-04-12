/* flux-isa-v3.h — ISA v3 Edge Encoding (variable-width 1-3 bytes)
 *
 * Format:
 *   Byte 0: [OOOO_KKKK] — opcode (high 4 bits) + continuation count (low 4 bits)
 *   Byte 1 (if K>=1): [DDDD_DDDD] — destination register or first operand
 *   Byte 2 (if K>=2): [SSSS_SSSS] — source register or immediate value
 *
 * Confidence fusion: opcodes 0x40-0x7F carry fused confidence in the low 2 bits
 * of byte 0's opcode field. E.g. CONF_MOVI = 0x5C, confidence bits = 0b00
 * means raw confidence propagated from accumulator.
 *
 * Register encoding: 0-63 maps to registers, 64-127 maps to immediates
 * (value = encoding - 64, range 0-63).
 */

#ifndef FLUX_ISA_V3_H
#define FLUX_ISA_V3_H

#include <stdint.h>
#include <stddef.h>

/* Opcode categories */
#define OP_CAT_NOP      0x0  /* 0x00-0x03: 1-byte instructions */
#define OP_CAT_STACK    0x1  /* 0x10-0x1F: 2-byte stack ops */
#define OP_CAT_ALU      0x2  /* 0x20-0x3F: 2-3 byte ALU */
#define OP_CAT_CONF     0x4  /* 0x40-0x5F: confidence-fused ALU */
#define OP_CAT_CTRL     0x6  /* 0x60-0x6F: 3-byte control flow */
#define OP_CAT_MEM      0x7  /* 0x70-0x7F: 3-byte memory */
#define OP_CAT_CONF_CTL 0x8  /* 0x80-0x8F: confidence + control */
#define OP_CAT_INSTINCT 0xA  /* 0xA0-0xAF: instinct ops */
#define OP_CAT_ENERGY   0xB  /* 0xB0-0xBF: energy ops */

/* Instruction lengths by opcode prefix */
static inline int isa_v3_length(uint8_t first_byte) {
    uint8_t op = first_byte & 0xF0;
    if (op <= 0x03) return 1;  /* NOP, HALT variants */
    if (op <= 0x50) return 2;  /* STACK, ALU, CONF */
    if (op == 0x40 || op == 0x50) return 2;  /* CONF ops */
    if (op >= 0x60) return 3;  /* CTRL, MEM, INSTINCT, ENERGY */
    return 2;  /* default */
}

/* Decode a 1-byte instruction */
typedef struct {
    uint8_t opcode;
    uint8_t cat;
    uint8_t rd;      /* destination register (0-63) or immediate (64-127) */
    uint8_t rs;      /* source register or immediate */
    uint8_t conf;    /* fused confidence (0-3, for CONF category) */
    int length;      /* instruction length in bytes */
} isa_v3_insn_t;

/* Decode instruction from bytecode buffer */
/* Returns instruction length, 0 on error */
static inline int isa_v3_decode(const uint8_t *buf, size_t buf_len, isa_v3_insn_t *out) {
    if (buf_len < 1) return 0;
    uint8_t b0 = buf[0];
    uint8_t cat = b0 >> 4;
    out->opcode = b0;
    out->cat = cat;
    out->rd = 0;
    out->rs = 0;
    out->conf = b0 & 0x03;  /* low 2 bits for CONF category */

    int len = isa_v3_length(b0);
    out->length = len;
    if (buf_len < (size_t)len) return 0;

    if (len >= 2) out->rd = buf[1];
    if (len >= 3) out->rs = buf[2];
    return len;
}

/* Encode instruction to bytecode buffer */
/* Returns bytes written, 0 on error */
static inline int isa_v3_encode(uint8_t opcode, uint8_t rd, uint8_t rs, uint8_t *buf, size_t buf_len) {
    int len = isa_v3_length(opcode);
    if (buf_len < (size_t)len) return 0;
    buf[0] = opcode;
    if (len >= 2) buf[1] = rd;
    if (len >= 3) buf[2] = rs;
    return len;
}

/* Build opcode with category + sub-opcode */
static inline uint8_t isa_v3_make_op(uint8_t category, uint8_t sub_op) {
    return (uint8_t)((category << 4) | (sub_op & 0x0F));
}

/* Build confidence-fused opcode */
static inline uint8_t isa_v3_make_conf_op(uint8_t category, uint8_t sub_op, uint8_t conf) {
    return (uint8_t)((category << 4) | ((sub_op & 0x0C)) | (conf & 0x03));
}

/* Extract sub-opcode from instruction (bits 3:2 for CONF, bits 3:0 otherwise) */
static inline uint8_t isa_v3_sub_op(uint8_t opcode, uint8_t cat) {
    if (cat >= 0x4 && cat <= 0x5) return (opcode >> 2) & 0x03;  /* CONF: skip conf bits */
    return opcode & 0x0F;
}

/* Register check: is this encoding a register (0-63) or immediate (64-127)? */
#define ISA_V3_IS_REG(x)    ((x) < 64)
#define ISA_V3_IS_IMM(x)    ((x) >= 64)
#define ISA_V3_IMM_VAL(x)   ((int)((x) - 64))
#define ISA_V3_MAKE_REG(r)  ((uint8_t)(r))
#define ISA_V3_MAKE_IMM(v)  ((uint8_t)((v) + 64))

#endif
