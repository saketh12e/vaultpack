/*
 * examples/streaming_pack_workflow.c
 *
 * Demonstrates the streaming pack context API: open a context,
 * begin several sections, feed each in two chunks, finish, and
 * emit a complete archive blob.
 *
 * Build: cc examples/streaming_pack_workflow.c src/*.c -I include -o /tmp/spw
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vaultpack/pack_ctx.h"

static void emit_section(vp_pack_ctx_t *ctx, const char *name,
                         const char *msg1, const char *msg2)
{
    if (vp_pack_ctx_begin(ctx, name, 1, 0) != VP_OK) {
        fprintf(stderr, "begin %s failed\n", name);
        return;
    }
    vp_pack_ctx_append(ctx, (const uint8_t *)msg1, strlen(msg1));
    if (msg2) vp_pack_ctx_append(ctx, (const uint8_t *)msg2, strlen(msg2));
    vp_pack_ctx_finish(ctx);
}

int main(void)
{
    vp_pack_ctx_t ctx;
    vp_pack_ctx_init(&ctx);

    emit_section(&ctx, "intro",
                 "VaultPack streaming pack demo\n", NULL);
    emit_section(&ctx, "config",
                 "version=1\n", "compression=lz4x\n");
    emit_section(&ctx, "payload",
                 "First chunk of payload.\n",
                 "Second chunk of payload.\n");

    uint8_t *out = NULL;
    size_t   out_len = 0;
    if (vp_pack_ctx_emit(&ctx, &out, &out_len) != VP_OK) {
        fprintf(stderr, "emit failed\n");
        vp_pack_ctx_destroy(&ctx);
        return 1;
    }
    printf("emitted %zu bytes; first 16: ", out_len);
    for (size_t i = 0; i < 16 && i < out_len; i++) printf("%02x ", out[i]);
    printf("\n");
    free(out);
    vp_pack_ctx_destroy(&ctx);
    return 0;
}