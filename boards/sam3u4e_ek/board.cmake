# SPDX-License-Identifier: Apache-2.0

# keep first
board_runner_args(jlink "--device=AT91SAM3U4E" "--speed=4000")

# keep first
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
