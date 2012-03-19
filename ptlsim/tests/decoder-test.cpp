
#include <gtest/gtest.h>

#define DISABLE_ASSERT
#include <decode.h>
#include <machine.h>
#include <ptlsim.h>

namespace {

    TEST(Decode, ModRM)
    {
        ModRMByte modrm;

#define check_modrm(m_t, r_t, rm_t) \
        ASSERT_EQ(m_t, modrm.mod); \
        ASSERT_EQ(r_t, modrm.reg); \
        ASSERT_EQ(rm_t, modrm.rm);

#define check_modrm_byte(m) \
        ASSERT_EQ(m, byte(modrm));

#define test_modrm(m, mod, reg, rm) \
        modrm = ModRMByte(m); \
        check_modrm(mod, reg, rm); \
        check_modrm_byte(m);

        test_modrm(0x01, 0, 0, 1);
        test_modrm(0x02, 0, 0, 2);
        test_modrm(0x03, 0, 0, 3);
        test_modrm(0x04, 0, 0, 4);
        test_modrm(0x05, 0, 0, 5);
        test_modrm(0x06, 0, 0, 6);
        test_modrm(0x07, 0, 0, 7);
        test_modrm(0x08, 0, 1, 0);
        test_modrm(0x09, 0, 1, 1);
        test_modrm(0x0a, 0, 1, 2);
        test_modrm(0x10, 0, 2, 0);
        test_modrm(0x18, 0, 3, 0);
        test_modrm(0x20, 0, 4, 0);
        test_modrm(0x28, 0, 5, 0);
        test_modrm(0x30, 0, 6, 0);
        test_modrm(0x38, 0, 7, 0);
        test_modrm(0x40, 1, 0, 0);
        test_modrm(0x41, 1, 0, 1);
        test_modrm(0x42, 1, 0, 2);
        test_modrm(0x48, 1, 1, 0);
        test_modrm(0x50, 1, 2, 0);
        test_modrm(0x80, 2, 0, 0);

    }

#ifdef INTEL_TSX
    Context* create_new_ctx(Context& src)
    {
        Context* ret = new Context();
        memcpy(ret, &src, sizeof(Context));

        ret->tsx_backup_ctx = new Context();
        bzero(ret->tsx_backup_ctx, sizeof(Context));

        return ret;
    }

    void delete_ctx(Context* ctx)
    {
        Context* tsx_ctx = ctx->tsx_backup_ctx;
        delete tsx_ctx;
        delete ctx;
    }

    TEST(DecodeTSX, TSX)
    {
        BaseMachine *base_machine = (BaseMachine*)PTLsimMachine::getmachine(
                "base");
        set_decoder_stats(base_machine, 0);

        RIPVirtPhys rvp;
        setzero(rvp);
        rvp.rip = 0x400200;

        TraceDecoder trans(rvp);
        trans.use64 = 1;

        byte insbuf[] = {
            0xc7, 0xf8, 0x34, 0x12, 0xff, 0xff, // XBEGIN
            0x0f, 0x01, 0xd6,                   // XTEST
            0x0f, 0x01, 0xd5,                   // XEND
            0xc6, 0xf8, 0x12,                   // XABORT
        };

        trans.insnbytes = insbuf;
        trans.insnbytes_bufsize = 15+15;
        trans.valid_byte_count = 16;

        for(;;) {
            if(!trans.translate()) break;
        }

        BasicBlock& bb = trans.bb;

        ASSERT_EQ(4, trans.user_insn_count);
        ASSERT_EQ(6, bb.count);
        ASSERT_EQ(0x40020f, trans.rip);

        foreach (i, bb.count) {
            TransOp& op = bb.transops[i];
            switch(i) {
                case 0:
                    ASSERT_EQ(OP_collcc, op.opcode);
                    ASSERT_TRUE(op.som);
                    break;
                case 1:
                    ASSERT_EQ(OP_ast, op.opcode);
                    ASSERT_EQ(L_ASSIST_XBEGIN, op.riptaken);
                    ASSERT_EQ((0x400206 - 60876), op.rbimm);
                    break;
                case 2:
                    ASSERT_EQ(OP_mf, op.opcode);
                    ASSERT_EQ(MF_TYPE_SFENCE, MF_TYPE_SFENCE & op.extshift);
                    ASSERT_EQ(MF_TYPE_LFENCE, MF_TYPE_LFENCE & op.extshift);
                    ASSERT_TRUE(op.eom);
                    break;
                case 3:
                    ASSERT_EQ(OP_ast, op.opcode);
                    ASSERT_EQ(L_ASSIST_XTEST, op.riptaken);
                    ASSERT_EQ(REG_zf, op.ra);
                    ASSERT_EQ(REG_zf, op.rd);
                    ASSERT_TRUE(op.som);
                    ASSERT_TRUE(op.eom);
                    break;
                case 4:
                    ASSERT_EQ(OP_ast, op.opcode);
                    ASSERT_EQ(L_ASSIST_XEND, op.riptaken);
                    ASSERT_EQ(REG_rax, op.ra);
                    ASSERT_EQ(REG_rax, op.rd);
                    ASSERT_TRUE(op.som);
                    ASSERT_TRUE(op.eom);
                    break;
                case 5:
                    ASSERT_EQ(OP_ast, op.opcode);
                    ASSERT_EQ(L_ASSIST_XABORT, op.riptaken);
                    ASSERT_EQ(REG_rax, op.ra);
                    ASSERT_EQ(REG_rax, op.rd);
                    ASSERT_EQ(0x12, op.rcimm);
                    ASSERT_TRUE(op.som);
                    ASSERT_TRUE(op.eom);
                    break;
            }
        }
    }

    TEST(DecodeTSX, BackupCtx)
    {
        Context& ctx = contextof(0);

        ASSERT_TRUE(ctx.tsx_backup_ctx);
    }

    TEST(DecodeTSX, Xbegin)
    {
        W64 rc;
        W16 flags;
        W64 abort_addr;

        Context& ctx = *create_new_ctx(contextof(0));
        Context& bctx = *ctx.tsx_backup_ctx;

        abort_addr = 0x412356;

        rc = l_assist_xbegin(ctx, 0, abort_addr, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, rc);
        ASSERT_EQ(1, ctx.tsx_mode);
        ASSERT_EQ(0, bctx.tsx_mode);

        ASSERT_EQ(abort_addr, ctx.tsx_abort_addr);

        foreach (i, TRANSREG_COUNT) {
            ASSERT_EQ(ctx.get(i), bctx.get(i)) << "Register " << arch_reg_names[i] <<
                " doesn't match.";
        }

        rc = l_assist_xbegin(ctx, 0, abort_addr + 0x10, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, rc);
        ASSERT_EQ(2, ctx.tsx_mode);
        ASSERT_EQ(0, bctx.tsx_mode);
        ASSERT_EQ(abort_addr, ctx.tsx_abort_addr);

        delete_ctx(&ctx);
    }

    TEST(DecodeTSX, Xend)
    {
        W64 rc;
        W64 rax;
        W16 flags;
        W64 abort_addr;

        Context& ctx = *create_new_ctx(contextof(0));
        Context& bctx = *ctx.tsx_backup_ctx;

        abort_addr = 0x412356;

        rc = l_assist_xbegin(ctx, 0, abort_addr, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, rc);
        ASSERT_EQ(1, ctx.tsx_mode);
        ASSERT_EQ(0, bctx.tsx_mode);

        rax = 0xffff777712345678;

        rc = l_assist_xend(ctx, rax, 0, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, ctx.tsx_mode);
        ASSERT_EQ(0, ctx.tsx_abort_addr);

        foreach (i, TRANSREG_COUNT) {
            ASSERT_EQ(0, bctx.get(i)) << "Register " << arch_reg_names[i] <<
                " is not cleared in backup ctx.";
        }

        ASSERT_EQ(rax, rc);

        delete_ctx(&ctx);
    }

    TEST(DecodeTSX, Xend_Recursive)
    {
        W64 rc;
        W64 rax;
        W16 flags;
        W64 abort_addr;

        Context& ctx = *create_new_ctx(contextof(0));
        Context& bctx = *ctx.tsx_backup_ctx;

        abort_addr = 0x412356;

        rc = l_assist_xbegin(ctx, 0, abort_addr, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, rc);
        ASSERT_EQ(1, ctx.tsx_mode);
        ASSERT_EQ(0, bctx.tsx_mode);

        rc = l_assist_xbegin(ctx, 0, abort_addr, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, rc);
        ASSERT_EQ(2, ctx.tsx_mode);
        ASSERT_EQ(0, bctx.tsx_mode);

        rax = 0xffff777712345678;

        rc = l_assist_xend(ctx, rax, 0, 0, 0, 0, 0, flags);

        ASSERT_EQ(1, ctx.tsx_mode);
        ASSERT_EQ(abort_addr, ctx.tsx_abort_addr);
        ASSERT_EQ(rax, rc);

        rax = 0xffff7777;

        rc = l_assist_xend(ctx, rax, 0, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, ctx.tsx_mode);
        ASSERT_EQ(0, ctx.tsx_abort_addr);
        ASSERT_EQ(rax, rc);

        foreach (i, TRANSREG_COUNT) {
            ASSERT_EQ(0, bctx.get(i)) << "Register " << arch_reg_names[i] <<
                " is not cleared in backup ctx.";
        }

        delete_ctx(&ctx);
    }

    TEST(DecodeTSX, Xend_abort)
    {
        W64 rc;
        W64 rb;
        W64 rax;
        W16 flags;
        W64 abort_addr;
        W64 updated_rax;

        Context& ctx = *create_new_ctx(contextof(0));
        Context& bctx = *ctx.tsx_backup_ctx;

        abort_addr = 0x412356;

        rc = l_assist_xbegin(ctx, 0, abort_addr, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, rc);
        ASSERT_EQ(1, ctx.tsx_mode);
        ASSERT_EQ(0, bctx.tsx_mode);

        rax = 0xffff777712345678;
        rb = 0x0b;
        updated_rax = (rax & ~(W64)(W32)(-1)) | (rb & (W32)(-1));
        updated_rax &= ~(0x1);

        rc = l_assist_xend(ctx, rax, rb, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, ctx.tsx_mode);
        ASSERT_EQ(0, ctx.tsx_abort_addr);
        ASSERT_EQ(abort_addr, ctx.reg_nextrip);

        ASSERT_EQ(updated_rax, rc);

        foreach (i, TRANSREG_COUNT) {
            if (i == REG_nextrip) continue;
            ASSERT_EQ(ctx.get(i), bctx.get(i)) << "Register " << arch_reg_names[i] <<
                " is not restored correctly.";
        }

        delete_ctx(&ctx);
    }

    TEST(DecodeTSX, Xend_abort_nested)
    {
        W64 rc;
        W64 rb;
        W64 rax;
        W16 flags;
        W64 abort_addr;
        W64 updated_rax;

        Context& ctx = *create_new_ctx(contextof(0));
        Context& bctx = *ctx.tsx_backup_ctx;

        abort_addr = 0x412356;

        rc = l_assist_xbegin(ctx, 0, abort_addr, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, rc);
        ASSERT_EQ(1, ctx.tsx_mode);
        ASSERT_EQ(0, bctx.tsx_mode);

        rc = l_assist_xbegin(ctx, 0, abort_addr, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, rc);
        ASSERT_EQ(2, ctx.tsx_mode);
        ASSERT_EQ(0, bctx.tsx_mode);

        rax = 0xffff777712345678;
        rb = 0x0b;
        updated_rax = (rax & ~(W64)(W32)(-1)) | (rb & (W32)(-1));
        updated_rax &= ~(0x1);
        updated_rax |= 0x20;

        rc = l_assist_xend(ctx, rax, rb, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, ctx.tsx_mode);
        ASSERT_EQ(0, ctx.tsx_abort_addr);
        ASSERT_EQ(abort_addr, ctx.reg_nextrip);

        ASSERT_EQ(updated_rax, rc);

        foreach (i, TRANSREG_COUNT) {
            if (i == REG_nextrip) continue;
            ASSERT_EQ(ctx.get(i), bctx.get(i)) << "Register " << arch_reg_names[i] <<
                " is not restored correctly.";
        }

        delete_ctx(&ctx);
    }

    TEST(DecodeTSX, Xabort)
    {
        W64 rc;
        W64 rb;
        W64 rax;
        W16 flags;
        W64 abort_addr;
        W64 updated_rax;

        Context& ctx = *create_new_ctx(contextof(0));
        Context& bctx = *ctx.tsx_backup_ctx;

        abort_addr = 0x412356;

        rc = l_assist_xbegin(ctx, 0, abort_addr, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, rc);
        ASSERT_EQ(1, ctx.tsx_mode);
        ASSERT_EQ(0, bctx.tsx_mode);

        rax = 0xffff777712345678;
        rb = 0x0b;
        rc = 0xaa;
        updated_rax = (rax & ~(W64)(W32)(-1)) | (rb & (W32)(-1));
        updated_rax = (updated_rax & ~(W64)(0xff000000)) | (
                rc << 24);

        rc = l_assist_xabort(ctx, rax, rb, rc, 0, 0, 0, flags);

        ASSERT_EQ(0, ctx.tsx_mode);
        ASSERT_EQ(0, ctx.tsx_abort_addr);
        ASSERT_EQ(abort_addr, ctx.reg_nextrip);

        ASSERT_EQ(updated_rax, rc);

        foreach (i, TRANSREG_COUNT) {
            if (i == REG_nextrip) continue;
            ASSERT_EQ(ctx.get(i), bctx.get(i)) << "Register " << arch_reg_names[i] <<
                " is not restored correctly.";
        }

        delete_ctx(&ctx);
    }

    TEST(DecodeTSX, Xtest)
    {
        W64 rc;
        W16 flags;

        Context& ctx = *create_new_ctx(contextof(0));
        Context& bctx = *ctx.tsx_backup_ctx;

        /* Xtest before starting tsx mode */
        rc = l_assist_xtest(ctx, 0, 0, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, rc);
        ASSERT_EQ(0, (flags & FLAG_ZF));

        /* Switch to tsx mode */
        rc = l_assist_xbegin(ctx, 0, 0x10, 0, 0, 0, 0, flags);

        rc = l_assist_xtest(ctx, 0, 0, 0, 0, 0, 0, flags);

        ASSERT_EQ(0, rc);
        ASSERT_EQ(FLAG_ZF, (flags & FLAG_ZF));

        delete_ctx(&ctx);
    }
#endif
};
