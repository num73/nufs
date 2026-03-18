/*
 * journal.c - Write-ahead journal for mmapfs crash consistency
 *
 * Implements a simple redo-log journal:
 *
 *   1. mmapfs_journal_begin()       – start a new transaction
 *   2. mmapfs_journal_write_block() – record a block write (can be
 *                                     called multiple times)
 *   3. mmapfs_journal_commit()      – persist the journal, apply
 *                                     changes, then clear the journal
 *
 * On recovery (after an unclean shutdown), mmapfs_journal_recover()
 * replays any committed-but-not-applied transaction.
 *
 * Journal layout on disk (inside the journal area):
 *   Block 0:   Descriptor (mmapfs_journal_desc)
 *   Block 1..N: Data blocks (one per journalled block write)
 *
 * Commit protocol:
 *   a) Flush data blocks in the journal area
 *   b) Set jd_committed = 1 and flush the descriptor  ← atomic commit
 *   c) Copy each data block from journal to its real target
 *   d) Flush real targets
 *   e) Clear the descriptor  ← checkpoint
 *
 * SPDX-License-Identifier: GPL-3.0
 */

#include "mmapfs.h"
#include <string.h>

int mmapfs_journal_init(void) {
    /* Nothing special – the journal area is already zeroed at format time
     * and is validated during recovery. */
    return 0;
}

int mmapfs_journal_begin(void) {
    struct mmapfs_state *st = mmapfs_get_state();
    struct mmapfs_super *sb = st->super;

    struct mmapfs_journal_desc *desc =
        (struct mmapfs_journal_desc *)mmapfs_get_block(sb->s_journal_start);

    desc->jd_magic = MMAPFS_JOURNAL_MAGIC;
    desc->jd_seq = ++sb->s_journal_seq;
    desc->jd_num_blocks = 0;
    desc->jd_committed = 0;

    return 0;
}

int mmapfs_journal_write_block(uint32_t block_no, const void *data) {
    struct mmapfs_state *st = mmapfs_get_state();
    struct mmapfs_super *sb = st->super;

    struct mmapfs_journal_desc *desc =
        (struct mmapfs_journal_desc *)mmapfs_get_block(sb->s_journal_start);

    if (desc->jd_num_blocks >= MMAPFS_JOURNAL_MAX_BLOCKS)
        return -ENOSPC;

    uint32_t idx = desc->jd_num_blocks;
    desc->jd_targets[idx] = block_no;

    /* Copy the new block data into the journal data area */
    void *jblock =
        mmapfs_get_block(sb->s_journal_start + 1 + idx);
    memcpy(jblock, data, MMAPFS_BLOCK_SIZE);

    desc->jd_num_blocks++;
    return 0;
}

int mmapfs_journal_commit(void) {
    struct mmapfs_state *st = mmapfs_get_state();
    struct mmapfs_super *sb = st->super;
    uint32_t js = sb->s_journal_start;

    struct mmapfs_journal_desc *desc =
        (struct mmapfs_journal_desc *)mmapfs_get_block(js);

    if (desc->jd_num_blocks == 0) {
        /* Empty transaction – nothing to do */
        desc->jd_magic = 0;
        return 0;
    }

    /* (a) Flush journal data blocks */
    mmapfs_sync_range(js + 1, desc->jd_num_blocks);

    /* (b) Atomic commit: set committed flag and flush descriptor */
    desc->jd_committed = 1;
    mmapfs_sync_block(js);

    /* (c) Apply: copy journal data to real target locations */
    for (uint32_t i = 0; i < desc->jd_num_blocks; i++) {
        void *src = mmapfs_get_block(js + 1 + i);
        void *dst = mmapfs_get_block(desc->jd_targets[i]);
        memcpy(dst, src, MMAPFS_BLOCK_SIZE);
    }

    /* (d) Flush real targets */
    for (uint32_t i = 0; i < desc->jd_num_blocks; i++) {
        mmapfs_sync_block(desc->jd_targets[i]);
    }

    /* (e) Checkpoint: clear journal */
    desc->jd_magic = 0;
    desc->jd_committed = 0;
    desc->jd_num_blocks = 0;
    mmapfs_sync_block(js);

    return 0;
}

int mmapfs_journal_recover(void) {
    struct mmapfs_state *st = mmapfs_get_state();
    struct mmapfs_super *sb = st->super;
    uint32_t js = sb->s_journal_start;

    struct mmapfs_journal_desc *desc =
        (struct mmapfs_journal_desc *)mmapfs_get_block(js);

    /* No valid journal header → nothing to recover */
    if (desc->jd_magic != MMAPFS_JOURNAL_MAGIC)
        return 0;

    /* Transaction was not committed → discard */
    if (!desc->jd_committed) {
        desc->jd_magic = 0;
        desc->jd_num_blocks = 0;
        mmapfs_sync_block(js);
        return 0;
    }

    /* Replay: copy each journal data block to its real target */
    for (uint32_t i = 0; i < desc->jd_num_blocks; i++) {
        void *src = mmapfs_get_block(js + 1 + i);
        void *dst = mmapfs_get_block(desc->jd_targets[i]);
        memcpy(dst, src, MMAPFS_BLOCK_SIZE);
        mmapfs_sync_block(desc->jd_targets[i]);
    }

    /* Clear journal */
    desc->jd_magic = 0;
    desc->jd_committed = 0;
    desc->jd_num_blocks = 0;
    mmapfs_sync_block(js);

    return 0;
}
