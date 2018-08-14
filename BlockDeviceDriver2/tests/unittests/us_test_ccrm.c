
#include "../../UserDaemon_sksrv.h"

// MOCK: (UserDaemon_sksrv.c) socket_send(skfd, buffer, total_len) => mock_socket_send(skfd, buffer, total_len)
void us_test_block_io_ret_ts(void)
{
    ptr = cmd_buffer;
    reqID = get_int(ptr);
    ptr += sizeof(int32_t);
    volID = get_int64(ptr);
    ptr += sizeof(uint64_t);

    cmd_do_block_io(0, DMS_CMD_BLOCK_VOLUME_IO)
}
