#include "stdafx.h"
#include "ai/move.h"
#include "EditorMap.h"
#include "EditorObject.h"
#include "ObjectLibrary.h"
#include "lib/pqueue.h"

#include <fstream>

namespace
{
    // clang-format off
    /// <summary>The overlay weights compiled into the editor; calcLinkCost restores part of the table from them.</summary>
    /// <remarks>
    /// The editor's read-only copy; <see cref="OverlayWeightTable"/> starts out as the same values. One row of nine
    /// cells per overlay, 75 overlays per move level.
    /// </remarks>
    constexpr std::array<int32_t, NUM_MOVE_LEVELS * OVERLAY_WEIGHT_LEVEL_SIZE> DefaultOverlayWeights = {
        // Move level 0
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 0
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 1
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 2
        0, -5, 0, -5, -5, 0, 0, 0, 0, // 3
        0, 0, 0, -5, -5, 0, 0, -5, 0, // 4
        0, 0, 0, 0, -5, -5, 0, -5, 0, // 5
        0, -5, 0, 0, -5, -5, 0, 0, 0, // 6
        0, -5, 0, -5, -5, -5, 0, -5, 0, // 7
        0, -5, 0, -5, -5, -5, 0, 0, 0, // 8
        0, -5, 0, -5, -5, 0, 0, -5, 0, // 9
        0, 0, 0, -5, -5, -5, 0, -5, 0, // 10
        0, -5, 0, 0, -5, -5, 0, -5, 0, // 11
        0, 0, 0, 0, -5, 0, 0, -5, 0, // 12
        0, 0, 0, -5, -5, 0, 0, 0, 0, // 13
        0, -5, 0, 0, -5, 0, 0, 0, 0, // 14
        0, 0, 0, 0, -5, -5, 0, 0, 0, // 15
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 16
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 17
        0, -5, 0, -5, -5, 0, 0, 0, 0, // 18
        0, 0, 0, -5, -5, 0, 0, -5, 0, // 19
        0, 0, 0, 0, -5, -5, 0, -5, 0, // 20
        0, -5, 0, 0, -5, -5, 0, 0, 0, // 21
        0, -5, 0, -5, -5, -5, 0, -5, 0, // 22
        0, -5, 0, -5, -5, -5, 0, 0, 0, // 23
        0, -5, 0, -5, -5, 0, 0, -5, 0, // 24
        0, 0, 0, -5, -5, -5, 0, -5, 0, // 25
        0, -5, 0, 0, -5, -5, 0, -5, 0, // 26
        0, 0, 0, 0, -5, 0, 0, -5, 0, // 27
        0, 0, 0, -5, -5, 0, 0, 0, 0, // 28
        0, -5, 0, 0, -5, 0, 0, 0, 0, // 29
        0, 0, 0, 0, -5, -5, 0, 0, 0, // 30
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 31
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 32
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 33
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 34
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 35
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 36
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 37
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 38
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 39
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 40
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 41
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 42
        0, -5, 0, -5, -5, 0, 0, 0, 0, // 43
        0, 0, 0, -5, -5, 0, 0, -5, 0, // 44
        0, 0, 0, 0, -5, -5, 0, -5, 0, // 45
        0, -5, 0, 0, -5, -5, 0, 0, 0, // 46
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 47
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 48
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 49
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 50
        0, 0, 0, 0, -5, 0, 0, -5, 0, // 51
        0, 0, 0, -5, -5, 0, 0, 0, 0, // 52
        0, -5, 0, 0, -5, 0, 0, 0, 0, // 53
        0, 0, 0, 0, -5, -5, 0, 0, 0, // 54
        10000, 5, 10000, 10000, 5, 10000, 10000, 5, 10000, // 55
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, // 56
        10000, 10000, 10000, 5, 5, 5, 10000, 10000, 10000, // 57
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, // 58
        15, 15, 15, 15, 15, 15, 15, 15, 15, // 59
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 60
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 61
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 62
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 63
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 64
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 65
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 66
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 67
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 68
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 69
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 70
        20000, 0, 20000, 20000, 0, 20000, 20000, 0, 20000, // 71
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 72
        20000, 20000, 20000, 0, 0, 0, 20000, 20000, 20000, // 73
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 74
        // Move level 1
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 0
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 1
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 2
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 3
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 4
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 5
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 6
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 7
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 10
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 11
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 12
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 13
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 14
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 15
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 16
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 17
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 18
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 19
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 20
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 21
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 22
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 23
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 24
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 25
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 26
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 27
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 28
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 29
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 30
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 31
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 32
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 33
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 34
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 35
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 36
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 37
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 38
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 39
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 40
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 41
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 42
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 43
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 44
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 45
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 46
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 47
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 48
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 49
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 50
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 51
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 52
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 53
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 54
        10000, 5, 10000, 10000, 5, 10000, 10000, 5, 10000, // 55
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, // 56
        10000, 10000, 10000, 5, 5, 5, 10000, 10000, 10000, // 57
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, // 58
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 59
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 60
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 61
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 62
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 63
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 64
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 65
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 66
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 67
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 68
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 69
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 70
        20000, 0, 20000, 20000, 0, 20000, 20000, 0, 20000, // 71
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 72
        20000, 20000, 20000, 0, 0, 0, 20000, 20000, 20000, // 73
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 74
        // Move level 2
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 0
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 1
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 2
        0, -5, 0, -5, -5, 0, 0, 0, 0, // 3
        0, 0, 0, -5, -5, 0, 0, -5, 0, // 4
        0, 0, 0, 0, -5, -5, 0, -5, 0, // 5
        0, -5, 0, 0, -5, -5, 0, 0, 0, // 6
        0, -5, 0, -5, -5, -5, 0, -5, 0, // 7
        0, -5, 0, -5, -5, -5, 0, 0, 0, // 8
        0, -5, 0, -5, -5, 0, 0, -5, 0, // 9
        0, 0, 0, -5, -5, -5, 0, -5, 0, // 10
        0, -5, 0, 0, -5, -5, 0, -5, 0, // 11
        0, 0, 0, 0, -5, 0, 0, -5, 0, // 12
        0, 0, 0, -5, -5, 0, 0, 0, 0, // 13
        0, -5, 0, 0, -5, 0, 0, 0, 0, // 14
        0, 0, 0, 0, -5, -5, 0, 0, 0, // 15
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 16
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 17
        0, -5, 0, -5, -5, 0, 0, 0, 0, // 18
        0, 0, 0, -5, -5, 0, 0, -5, 0, // 19
        0, 0, 0, 0, -5, -5, 0, -5, 0, // 20
        0, -5, 0, 0, -5, -5, 0, 0, 0, // 21
        0, -5, 0, -5, -5, -5, 0, -5, 0, // 22
        0, -5, 0, -5, -5, -5, 0, 0, 0, // 23
        0, -5, 0, -5, -5, 0, 0, -5, 0, // 24
        0, 0, 0, -5, -5, -5, 0, -5, 0, // 25
        0, -5, 0, 0, -5, -5, 0, -5, 0, // 26
        0, 0, 0, 0, -5, 0, 0, -5, 0, // 27
        0, 0, 0, -5, -5, 0, 0, 0, 0, // 28
        0, -5, 0, 0, -5, 0, 0, 0, 0, // 29
        0, 0, 0, 0, -5, -5, 0, 0, 0, // 30
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 31
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 32
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 33
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 34
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 35
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 36
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 37
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 38
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 39
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 40
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 41
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 42
        0, -5, 0, -5, -5, 0, 0, 0, 0, // 43
        0, 0, 0, -5, -5, 0, 0, -5, 0, // 44
        0, 0, 0, 0, -5, -5, 0, -5, 0, // 45
        0, -5, 0, 0, -5, -5, 0, 0, 0, // 46
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 47
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 48
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 49
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 50
        0, 0, 0, 0, -5, 0, 0, -5, 0, // 51
        0, 0, 0, -5, -5, 0, 0, 0, 0, // 52
        0, -5, 0, 0, -5, 0, 0, 0, 0, // 53
        0, 0, 0, 0, -5, -5, 0, 0, 0, // 54
        10000, 5, 10000, 10000, 5, 10000, 10000, 5, 10000, // 55
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, // 56
        10000, 10000, 10000, 5, 5, 5, 10000, 10000, 10000, // 57
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, // 58
        15, 15, 15, 15, 15, 15, 15, 15, 15, // 59
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 60
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 61
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 62
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 63
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 64
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 65
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 66
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 67
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 68
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 69
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 70
        20000, 0, 20000, 20000, 0, 20000, 20000, 0, 20000, // 71
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 72
        20000, 20000, 20000, 0, 0, 0, 20000, 20000, 20000, // 73
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 74
        // Move level 3
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 0
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 1
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 2
        0, -5, 0, -5, -5, 0, 0, 0, 0, // 3
        0, 0, 0, -5, -5, 0, 0, -5, 0, // 4
        0, 0, 0, 0, -5, -5, 0, -5, 0, // 5
        0, -5, 0, 0, -5, -5, 0, 0, 0, // 6
        0, -5, 0, -5, -5, -5, 0, -5, 0, // 7
        0, -5, 0, -5, -5, -5, 0, 0, 0, // 8
        0, -5, 0, -5, -5, 0, 0, -5, 0, // 9
        0, 0, 0, -5, -5, -5, 0, -5, 0, // 10
        0, -5, 0, 0, -5, -5, 0, -5, 0, // 11
        0, 0, 0, 0, -5, 0, 0, -5, 0, // 12
        0, 0, 0, -5, -5, 0, 0, 0, 0, // 13
        0, -5, 0, 0, -5, 0, 0, 0, 0, // 14
        0, 0, 0, 0, -5, -5, 0, 0, 0, // 15
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 16
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 17
        0, -5, 0, -5, -5, 0, 0, 0, 0, // 18
        0, 0, 0, -5, -5, 0, 0, -5, 0, // 19
        0, 0, 0, 0, -5, -5, 0, -5, 0, // 20
        0, -5, 0, 0, -5, -5, 0, 0, 0, // 21
        0, -5, 0, -5, -5, -5, 0, -5, 0, // 22
        0, -5, 0, -5, -5, -5, 0, 0, 0, // 23
        0, -5, 0, -5, -5, 0, 0, -5, 0, // 24
        0, 0, 0, -5, -5, -5, 0, -5, 0, // 25
        0, -5, 0, 0, -5, -5, 0, -5, 0, // 26
        0, 0, 0, 0, -5, 0, 0, -5, 0, // 27
        0, 0, 0, -5, -5, 0, 0, 0, 0, // 28
        0, -5, 0, 0, -5, 0, 0, 0, 0, // 29
        0, 0, 0, 0, -5, -5, 0, 0, 0, // 30
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 31
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 32
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 33
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 34
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 35
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 36
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 37
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 38
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 39
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 40
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 41
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 42
        0, -5, 0, -5, -5, 0, 0, 0, 0, // 43
        0, 0, 0, -5, -5, 0, 0, -5, 0, // 44
        0, 0, 0, 0, -5, -5, 0, -5, 0, // 45
        0, -5, 0, 0, -5, -5, 0, 0, 0, // 46
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 47
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 48
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 49
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 50
        0, 0, 0, 0, -5, 0, 0, -5, 0, // 51
        0, 0, 0, -5, -5, 0, 0, 0, 0, // 52
        0, -5, 0, 0, -5, 0, 0, 0, 0, // 53
        0, 0, 0, 0, -5, -5, 0, 0, 0, // 54
        10000, 5, 10000, 10000, 5, 10000, 10000, 5, 10000, // 55
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, // 56
        10000, 10000, 10000, 5, 5, 5, 10000, 10000, 10000, // 57
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, // 58
        15, 15, 15, 15, 15, 15, 15, 15, 15, // 59
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 60
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 61
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 62
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 63
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 64
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 65
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 66
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 67
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 68
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 69
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 70
        20000, 0, 20000, 20000, 0, 20000, 20000, 0, 20000, // 71
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 72
        20000, 20000, 20000, 0, 0, 0, 20000, 20000, 20000, // 73
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 74
        // Move level 4
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 0
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 1
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 2
        0, -5, 0, -5, -5, 0, 0, 0, 0, // 3
        0, 0, 0, -5, -5, 0, 0, -5, 0, // 4
        0, 0, 0, 0, -5, -5, 0, -5, 0, // 5
        0, -5, 0, 0, -5, -5, 0, 0, 0, // 6
        0, -5, 0, -5, -5, -5, 0, -5, 0, // 7
        0, -5, 0, -5, -5, -5, 0, 0, 0, // 8
        0, -5, 0, -5, -5, 0, 0, -5, 0, // 9
        0, 0, 0, -5, -5, -5, 0, -5, 0, // 10
        0, -5, 0, 0, -5, -5, 0, -5, 0, // 11
        0, 0, 0, 0, -5, 0, 0, -5, 0, // 12
        0, 0, 0, -5, -5, 0, 0, 0, 0, // 13
        0, -5, 0, 0, -5, 0, 0, 0, 0, // 14
        0, 0, 0, 0, -5, -5, 0, 0, 0, // 15
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 16
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 17
        0, -5, 0, -5, -5, 0, 0, 0, 0, // 18
        0, 0, 0, -5, -5, 0, 0, -5, 0, // 19
        0, 0, 0, 0, -5, -5, 0, -5, 0, // 20
        0, -5, 0, 0, -5, -5, 0, 0, 0, // 21
        0, -5, 0, -5, -5, -5, 0, -5, 0, // 22
        0, -5, 0, -5, -5, -5, 0, 0, 0, // 23
        0, -5, 0, -5, -5, 0, 0, -5, 0, // 24
        0, 0, 0, -5, -5, -5, 0, -5, 0, // 25
        0, -5, 0, 0, -5, -5, 0, -5, 0, // 26
        0, 0, 0, 0, -5, 0, 0, -5, 0, // 27
        0, 0, 0, -5, -5, 0, 0, 0, 0, // 28
        0, -5, 0, 0, -5, 0, 0, 0, 0, // 29
        0, 0, 0, 0, -5, -5, 0, 0, 0, // 30
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 31
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 32
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 33
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 34
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 35
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 36
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 37
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 38
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 39
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 40
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 41
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 42
        0, -5, 0, -5, -5, 0, 0, 0, 0, // 43
        0, 0, 0, -5, -5, 0, 0, -5, 0, // 44
        0, 0, 0, 0, -5, -5, 0, -5, 0, // 45
        0, -5, 0, 0, -5, -5, 0, 0, 0, // 46
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 47
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 48
        0, -5, 0, 0, -5, 0, 0, -5, 0, // 49
        0, 0, 0, -5, -5, -5, 0, 0, 0, // 50
        0, 0, 0, 0, -5, 0, 0, -5, 0, // 51
        0, 0, 0, -5, -5, 0, 0, 0, 0, // 52
        0, -5, 0, 0, -5, 0, 0, 0, 0, // 53
        0, 0, 0, 0, -5, -5, 0, 0, 0, // 54
        10000, 5, 10000, 10000, 5, 10000, 10000, 5, 10000, // 55
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, // 56
        10000, 10000, 10000, 5, 5, 5, 10000, 10000, 10000, // 57
        10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, 10000, // 58
        15, 15, 15, 15, 15, 15, 15, 15, 15, // 59
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 60
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 61
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 62
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 63
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 64
        -5, -5, -5, -5, -5, -5, -5, -5, -5, // 65
        0, 0, 0, 0, 0, 0, 0, 0, 0, // 66
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 67
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 68
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 69
        20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, 20000, // 70
        20000, 0, 20000, 20000, 0, 20000, 20000, 0, 20000, // 71
        20000, 5, 20000, 20000, 5, 20000, 20000, 5, 20000, // 72
        20000, 20000, 20000, 0, 0, 0, 20000, 20000, 20000, // 73
        20000, 20000, 20000, 5, 5, 5, 20000, 20000, 20000, // 74
    };

    // clang-format on

    /// <summary>
    /// Per gate overlay (67..74) and team alignment + 1, the overlay the gate behaves as for that team, or -1 when it
    /// is closed to it.
    /// </summary>
    constexpr int32_t GateOverlayForAlignment[3][8] = {
        {71, 72, 73, 74, 67, 68, 69, 70},
        {-1, -1, -1, -1, -1, -1, -1, -1},
        {67, 68, 69, 70, 71, 72, 73, 74},
    };

    /// <summary>First overlay type of the gates.</summary>
    constexpr uint32_t FIRST_GATE_OVERLAY = 0x43;
    /// <summary>Last overlay type of the gates.</summary>
    constexpr uint32_t LAST_GATE_OVERLAY = 0x4a;
    /// <summary>The team alignment the editor's cell search treats gates for (the game asks the moving object).</summary>
    constexpr int32_t EDITOR_GATE_ALIGNMENT = 1;

    /// <summary>The first overlay of the dirt roads calcLinkCost resets (overlays 1..15).</summary>
    constexpr int32_t FIRST_DIRT_ROAD_OVERLAY = 1;
    /// <summary>How many dirt road overlays calcLinkCost resets.</summary>
    constexpr int32_t NUM_DIRT_ROAD_OVERLAYS = 15;

    /// <summary>
    /// The weights ClearBridgeTiles lowers by 10000 in the first move level of the overlay weights (the centre cells
    /// of the two bridge overlays and of the two railroad bridges).
    /// </summary>
    constexpr int32_t BRIDGE_WEIGHT_CELLS[12] = {334, 337, 340, 352, 355, 358, 496, 499, 502, 514, 517, 520};

    /// <summary>Cost of a blocked cell; the searches skip cells that cost this or more.</summary>
    constexpr int32_t BLOCKED_CELL_COST = 10000;
    /// <summary>Cost added to every cell of a gate closed to the mover.</summary>
    constexpr int32_t CLOSED_GATE_COST = 20000;
    /// <summary>calcLinkCost's result when the cell search finds no path.</summary>
    constexpr int32_t NO_LINK_COST = 9999;
    /// <summary>The move level calcLinkCost searches at (a plain cell's cost).</summary>
    constexpr int32_t LINK_MOVE_LEVEL = 10;
    /// <summary>The neighbour offsets calcLinkCost searches (no jumps).</summary>
    constexpr int32_t LINK_NUM_OFFSETS = 8;

    /// <summary>Capacity of the shared open list.</summary>
    constexpr int32_t OPEN_LIST_SIZE = 5000;
    /// <summary>The open list's sentinel key, below every real one.</summary>
    constexpr int32_t OPEN_LIST_KEY_MIN = -2000000;

    /// <summary>MapTile overlay bits: the overlay type.</summary>
    constexpr uint32_t OVERLAY_TYPE_MASK = 0x7f;
    /// <summary>MapTile overlay bits: the nine path-lock bits start here.</summary>
    constexpr uint32_t OVERLAY_LOCK_SHIFT = 15;
    /// <summary>MoveMap::setUp params bit: add the path locks' cost.</summary>
    constexpr uint32_t PARAM_USE_LOCKS = 0x80;

    /// <summary>Area map value of a tile in no area yet.</summary>
    constexpr int16_t AREA_NONE = -1;
    /// <summary>Area map value of a tile that can't be in any area.</summary>
    constexpr int16_t AREA_BLOCKED = -2;
    /// <summary>Cells per row of calcGlobalDoors's cell map (120 x 120 on the stack).</summary>
    constexpr int32_t DOOR_CELL_MAP_SIDE = 120;
    /// <summary>Most areas the small (byte) area map can number; 0xff is "none".</summary>
    constexpr int32_t SMALL_AREA_MAP_LIMIT = 256;

    /// <summary>A* list flags of doors and cells.</summary>
    constexpr uint32_t ON_OPEN_LIST = 1;
    constexpr uint32_t ON_CLOSED_LIST = 2;
    /// <summary>MoveMap cell flags: on the found path, a goal cell, a mover stands here.</summary>
    constexpr uint32_t CELL_ON_PATH = 4;
    constexpr uint32_t CELL_GOAL = 8;
    constexpr uint32_t CELL_HAS_MOVER = 0x10;

    /// <summary>The largest step count <see cref="MovePath::init"/> has seen.</summary>
    int32_t maxMovePathSteps = 0;
    /// <summary>Whether cellShiftDistance has been filled.</summary>
    int cellShiftDistanceReady = 0;

    /// <summary>Whether a tile's cell (cellR, cellC) is passable.</summary>
    uint32_t TileCellPassable(const MapTile& tile, int32_t cellR, int32_t cellC)
    {
        const uint32_t shift = static_cast<uint32_t>((cellR * MAPCELL_DIM + cellC) * 2);
        return (tile.cells & (0x4000u << shift)) >> (shift + 14);
    }

    /// <summary>A tile's overlay type.</summary>
    uint32_t TileOverlay(ScenarioMap* scenarioMap, int32_t row, int32_t col)
    {
        return scenarioMap->map[scenarioMap->width * row + col].overlay & OVERLAY_TYPE_MASK;
    }

    /// <summary>A cost plus <paramref name="delta"/>, at least 1.</summary>
    int32_t AddCost(int32_t cost, int32_t delta)
    {
        const int32_t result = cost + delta;
        return result < 1 ? 1 : result;
    }

    /// <summary>Adds <paramref name="delta"/> to the bridge cells of <paramref name="weights"/>.</summary>
    void AdjustBridgeWeights(int32_t* weights, int32_t delta)
    {
        for (const int32_t cell : BRIDGE_WEIGHT_CELLS)
        {
            weights[cell] += delta;
        }
    }

    /// <summary>The open-list id of a MoveMap cell.</summary>
    int32_t CellId(int32_t r, int32_t c)
    {
        return c + r * 1000;
    }

    /// <summary>
    /// The cost of stepping into a cell of <paramref name="cellCost"/> by offset <paramref name="offset"/>: half again
    /// for a diagonal, plus jumpCost (or only jumpCost, with JumpOnBlocked) for a jump.
    /// </summary>
    int32_t StepCost(int32_t cellCost, int32_t offset, int32_t jumpCost)
    {
        if (offset < 8)
        {
            return IsDiagonalStep[offset] != 0 ? cellCost + cellCost / 2 : cellCost;
        }

        if (JumpOnBlocked == 0)
        {
            return cellCost + jumpCost;
        }

        return jumpCost;
    }

    /// <summary>The world x of the middle of map cell column <paramref name="cellCol"/>.</summary>
    float CellColToWorldX(int32_t cellCol)
    {
        return static_cast<float>((static_cast<double>(cellCol) + 0.5f) * MetersPerCell - worldUnitsMapSide * 0.5f);
    }

    /// <summary>The world y of the middle of map cell row <paramref name="cellRow"/>.</summary>
    float CellRowToWorldY(int32_t cellRow)
    {
        const double mapHalf = worldUnitsMapSide * 0.5f;
        return static_cast<float>((mapHalf - static_cast<double>(cellRow) * MetersPerCell) - MetersPerCell * 0.5f);
    }

    /// <summary>Makes the shared open list on first use.</summary>
    void CreateOpenList()
    {
        if (openList == nullptr)
        {
            openList = new PriorityQueue;
            openList->init(OPEN_LIST_SIZE, OPEN_LIST_KEY_MIN);
        }
    }

    /// <summary>Writes a value to a CFile as its raw bytes.</summary>
    template <typename T> void WriteValue(CFile* file, const T& value)
    {
        file->Write(&value, sizeof(T));
    }

    /// <summary>Writes a field into a file record and steps past it.</summary>
    template <typename T> void WriteRecordField(uint8_t*& cursor, T value)
    {
        std::memcpy(cursor, &value, sizeof(T));
        cursor += sizeof(T);
    }

    /// <summary>Encodes an area as its 0x29-byte file record (the doors pointer written as 0).</summary>
    void EncodeArea(const GlobalMapArea& area, uint8_t* record)
    {
        WriteRecordField<int16_t>(record, area.sectorR);
        WriteRecordField<int16_t>(record, area.sectorC);
        WriteRecordField<uint32_t>(record, 0);
        WriteRecordField<int32_t>(record, area.type);
        WriteRecordField<char>(record, area.numDoors);
        WriteRecordField<int32_t>(record, area.open);
        WriteRecordField<int32_t>(record, GLOBALMAP_AREA_SPARE[0]);
        WriteRecordField<int32_t>(record, GLOBALMAP_AREA_SPARE[1]);
        WriteRecordField<int32_t>(record, area.closed);
        WriteRecordField<int32_t>(record, GLOBALMAP_AREA_SPARE[2]);
        WriteRecordField<int32_t>(record, GLOBALMAP_AREA_SPARE[3]);
        WriteRecordField<int32_t>(record, GLOBALMAP_AREA_SPARE[4]);
    }

    /// <summary>Encodes a door as its 0x3b-byte file record (the link pointers written as 0).</summary>
    void EncodeDoor(const GlobalMapDoor& door, uint8_t* record)
    {
        WriteRecordField<int16_t>(record, door.row);
        WriteRecordField<int16_t>(record, door.col);
        WriteRecordField<uint8_t>(record, door.cellR);
        WriteRecordField<uint8_t>(record, door.cellC);
        WriteRecordField<char>(record, door.length);
        WriteRecordField<int32_t>(record, door.open);
        WriteRecordField<int16_t>(record, door.area[0]);
        WriteRecordField<int16_t>(record, door.area[1]);
        WriteRecordField<int16_t>(record, door.areaCost[0]);
        WriteRecordField<int16_t>(record, door.areaCost[1]);
        WriteRecordField<char>(record, door.direction[0]);
        WriteRecordField<char>(record, door.direction[1]);
        WriteRecordField<char>(record, door.numLinks[0]);
        WriteRecordField<char>(record, door.numLinks[1]);
        WriteRecordField<uint32_t>(record, 0);
        WriteRecordField<uint32_t>(record, 0);
        WriteRecordField<int32_t>(record, door.cost);
        WriteRecordField<int32_t>(record, door.parent);
        WriteRecordField<int32_t>(record, door.fromAreaIndex);
        WriteRecordField<uint32_t>(record, door.flags);
        WriteRecordField<int32_t>(record, door.g);
        WriteRecordField<int32_t>(record, door.hPrime);
        WriteRecordField<int32_t>(record, door.fPrime);
    }

    /// <summary>Whether (row, col) lies in the sector being filled (GlobalMap's fill bounds).</summary>
    bool InFillSector(int32_t row, int32_t col);

    /// <summary>Allocates <paramref name="count"/> zeroed <typeparamref name="T"/>s with calloc.</summary>
    /// <remarks>
    /// The editor uses malloc, so the fields it never sets (an area's word at +0x15, the spare link slots, the two
    /// temporary doors) are heap garbage in the file it writes; the rebuild writes zeros there (EB-017).
    /// </remarks>
    template <typename T> T* AllocZeroed(size_t count)
    {
        return static_cast<T*>(std::calloc(count, sizeof(T)));
    }
}

int BlockWallTiles = 1;
int32_t SimpleMovePathRange = 10;
float metersPerWorldUnit = 0.2f;
int32_t cellShift[NUM_CELL_OFFSETS * 2] = {
    -1,  0, -1,  1,   0, 1,   1,   1,   1,   0,  1,   -1,  0,  -1,  -1,  -1,  -2,  0, -2,  2,   0, 2,   2,   2,
    2,   0, 2,   -2,  0, -2,  -2,  -2,  -3,  0,  -3,  3,   0,  3,   3,   3,   3,   0, 3,   -3,  0, -3,  -3,  -3,
    -3,  2, -2,  3,   2, 3,   3,   2,   3,   -2, 2,   -3,  -2, -3,  -3,  -2,  -4,  0, -4,  4,   0, 4,   4,   4,
    4,   0, 4,   -4,  0, -4,  -4,  -4,  -5,  0,  -5,  5,   0,  5,   5,   5,   5,   0, 5,   -5,  0, -5,  -5,  -5,
    -6,  0, -6,  6,   0, 6,   6,   6,   6,   0,  6,   -6,  0,  -6,  -6,  -6,  -7,  0, -7,  7,   0, 7,   7,   7,
    7,   0, 7,   -7,  0, -7,  -7,  -7,  -8,  0,  -8,  8,   0,  8,   8,   8,   8,   0, 8,   -8,  0, -8,  -8,  -8,
    -9,  0, -9,  9,   0, 9,   9,   9,   9,   0,  9,   -9,  0,  -9,  -9,  -9,  -10, 0, -10, 10,  0, 10,  10,  10,
    10,  0, 10,  -10, 0, -10, -10, -10, -11, 0,  -11, 11,  0,  11,  11,  11,  11,  0, 11,  -11, 0, -11, -11, -11,
    -12, 0, -12, 12,  0, 12,  12,  12,  12,  0,  12,  -12, 0,  -12, -12, -12,
};

char reverseShift[NUM_CELL_OFFSETS] = {
    4,  5,  6,  7,  0,  1,  2,  3,  12, 13, 14, 15, 8,  9,  10, 11, 20, 21, 22,  23,  16,  17,  18, 19, 28, 29,
    30, 31, 24, 25, 26, 27, 36, 37, 38, 39, 32, 33, 34, 35, 44, 45, 46, 47, 40,  41,  42,  43,  52, 53, 54, 55,
    48, 49, 50, 51, 60, 61, 62, 63, 56, 57, 58, 59, 68, 69, 70, 71, 64, 65, 66,  67,  76,  77,  78, 79, 72, 73,
    74, 75, 84, 85, 86, 87, 80, 81, 82, 83, 92, 93, 94, 95, 88, 89, 90, 91, 100, 101, 102, 103, 96, 97, 98, 99,
};

int IsDiagonalStep[NUM_CELL_OFFSETS] = {0, 1, 0, 1, 0, 1, 0, 1};
int32_t StepAdjDir[9] = {-1, 0, 2, 2, 4, 4, 6, 6, 0};
int32_t adjTile[4][2] = {{-1, 0}, {0, 1}, {1, 0}, {0, -1}};
char mineLayout[4][9] = {{}, {}, {1, 0, 1, 0, 1, 0, 1, 0, 1}, {}};
int32_t MaxHPrime = 1000;
std::array<int32_t, NUM_MOVE_LEVELS * OVERLAY_WEIGHT_LEVEL_SIZE> OverlayWeightTable = DefaultOverlayWeights;
int32_t OverlayWeightIndex[NUM_OVERLAY_TYPES] = {};
float cellToWorldCoord[MAPCELL_DIM] = {};
int32_t tileMulMAPCELL_DIM[MAX_MAP_TILE_WIDTH] = {};
float cellShiftDistance[NUM_CELL_OFFSETS] = {};
float tileRowToWorldCoord[MAX_MAP_TILE_WIDTH] = {};
float tileColToWorldCoord[MAX_MAP_TILE_WIDTH] = {};
float worldUnitsMapSide = 0.0f;
int ClearBridgeTiles = 0;
PriorityQueue* openList = nullptr;
int JumpOnBlocked = 0;
int FindingEscapePath = 0;
ScenarioMap* GameMap = nullptr;
GlobalMap* GlobalMoveMap = nullptr;
int32_t CurPlanet = 0;
float MapCellDiagonal = 0.0f;
float HalfMapCell = 0.0f;
float VerticesMapSideDivTwo = 0.0f;
float MetersMapSideDivTwo = 0.0f;
int GoalIsDoor = 0;
float MetersPerCell = 0.0f;
MoveMap* PathFindMap = nullptr;

namespace
{
    /// <summary>Tile bounds of the sector being filled.</summary>
    int32_t minTileR = 0;
    int32_t maxTileR = 0;
    int32_t minTileC = 0;
    int32_t maxTileC = 0;

    bool InFillSector(int32_t row, int32_t col)
    {
        return row >= minTileR && row < maxTileR && col >= minTileC && col < maxTileC;
    }
}

//---------------------------------------------------------------------------
// ScenarioMap

void ScenarioMap::init(int32_t newWidth, int32_t newHeight)
{
    const float cellSide = metersPerVertex * (1.0f / 3.0f);

    for (int32_t i = 0; i < MAX_MAP_TILE_WIDTH; i++)
    {
        tileMulMAPCELL_DIM[i] = i * MAPCELL_DIM;
    }

    MetersMapSideDivTwo = static_cast<float>(newWidth) * metersPerVertex;
    worldUnitsMapSide = MetersMapSideDivTwo;
    MetersPerCell = cellSide;

    for (int32_t row = 0; row < newHeight; row++)
    {
        tileRowToWorldCoord[row] =
            static_cast<float>(MetersMapSideDivTwo * 0.5 - static_cast<double>(row) * metersPerVertex);
    }

    for (int32_t col = 0; col < newWidth; col++)
    {
        tileColToWorldCoord[col] =
            static_cast<float>(static_cast<double>(col) * metersPerVertex - worldUnitsMapSide * 0.5);
    }

    for (int32_t cell = 0; cell < MAPCELL_DIM; cell++)
    {
        cellToWorldCoord[cell] = static_cast<float>(cell) * cellSide;
    }

    VerticesMapSideDivTwo = static_cast<float>(static_cast<double>(newWidth) * 0.5);
    MetersMapSideDivTwo = MetersMapSideDivTwo * 0.5f;
    MapCellDiagonal = static_cast<float>(static_cast<double>(cellSide) * metersPerWorldUnit * 1.4142);
    HalfMapCell = static_cast<float>(cellSide * 0.5);
    width = newWidth;
    height = newHeight;
    map = AllocZeroed<MapTile>(static_cast<size_t>(newWidth * newHeight));
    pathMap = AllocZeroed<uint8_t>(static_cast<size_t>(newWidth * newHeight));
}

int32_t ScenarioMap::write(CFile* mapFile)
{
    WriteValue(mapFile, height);
    WriteValue(mapFile, width);
    WriteValue(mapFile, baseElevation);
    mapFile->Write(map, static_cast<UINT>(height * width * sizeof(MapTile)));
    return 1;
}

void ScenarioMap::destroy()
{
    if (map != nullptr)
    {
        std::free(map);
        map = nullptr;
    }

    if (pathMap != nullptr)
    {
        std::free(pathMap);
        pathMap = nullptr;
    }
}

void ScenarioMap::worldToMapPos(vector_3d pos, int32_t& tileR, int32_t& tileC, int32_t& cellR, int32_t& cellC)
{
    const double oneOverMetersPerVertex = 1.0f / metersPerVertex;
    tileC = static_cast<int16_t>(static_cast<int32_t>(oneOverMetersPerVertex * pos.x + VerticesMapSideDivTwo));
    tileR = static_cast<int16_t>(
        static_cast<int32_t>((static_cast<double>(MetersMapSideDivTwo) - pos.y) * oneOverMetersPerVertex));
    cellC = static_cast<int32_t>((static_cast<double>(pos.x) - tileColToWorldCoord[tileC]) / MetersPerCell);
    cellR = static_cast<int32_t>((static_cast<double>(tileRowToWorldCoord[tileR]) - pos.y) / MetersPerCell);
}

int ScenarioMap::inBounds(int32_t tileR, int32_t tileC)
{
    return tileR >= 0 && tileR < height && tileC >= 0 && tileC < width ? 1 : 0;
}

MapTile ScenarioMap::getTile(int32_t tileR, int32_t tileC)
{
    return map[width * tileR + tileC];
}

//---------------------------------------------------------------------------
// Tile and overlay types

int32_t calcTileTypeFromIndex(int32_t tileIndex)
{
    // Forty bands of 79 texture indices (types 1..40), then the smaller special bands.
    for (int32_t band = 1; band <= 40; band++)
    {
        if (tileIndex < band * 0x4f)
        {
            return band;
        }
    }

    static constexpr struct
    {
        int32_t limit;
        int32_t type;
    } bands[] = {
        {0xc6e, 0x29}, {0xc84, 0x2a}, {0xc88, 0x2b}, {0xc96, 0x2c}, {0xca4, 0x2d}, {0xcb0, 0x2e},
        {0xcbc, 0x2f}, {0xcc8, 0x30}, {0xcd4, 0x31}, {0xce0, 0x32}, {0xcec, 0x33}, {0xcf4, 0x34},
        {0xcfc, 0x35}, {0xd15, 0x3a}, {0xd22, 0x36}, {0xd30, 0x38}, {0xd3e, 0x37},
    };

    for (const auto& band : bands)
    {
        if (tileIndex < band.limit)
        {
            return band.type;
        }
    }

    return tileIndex > 0xd65 ? 0 : 2;
}

int32_t calcOverlayTypeFromIndex(int32_t overlayIndex)
{
    if (overlayIndex == 0x29)
    {
        return 0;
    }

    if ((overlayIndex > 0xd01 && overlayIndex < 0xd0a) || (overlayIndex > 0xe7d && overlayIndex < 0xe82))
    {
        return 0x3b;
    }

    if ((overlayIndex > 0xd09 && overlayIndex < 0xd0e) || (overlayIndex > 0xd65 && overlayIndex < 0xd6e))
    {
        return 0x3e;
    }

    if ((overlayIndex > 0xd0d && overlayIndex < 0xd12) || (overlayIndex > 0xd6d && overlayIndex < 0xd76))
    {
        return 0x3f;
    }

    if (overlayIndex < 0xd51 || (overlayIndex > 0xda7 && overlayIndex < 0xdbc))
    {
        return 0x3c;
    }

    if (overlayIndex < 0xd64 || (overlayIndex > 0xdbb && overlayIndex < 0xdce))
    {
        return 0x3d;
    }

    if (overlayIndex < 0xd82)
    {
        return 0x42;
    }

    if (overlayIndex < 0xd95)
    {
        return 0x40;
    }

    if (overlayIndex < 0xda8)
    {
        return 0x41;
    }

    if (overlayIndex < 0xdcd)
    {
        return 0;
    }

    if (overlayIndex < 0xde1)
    {
        if (overlayIndex < 0xdd1)
        {
            return 1;
        }

        if (overlayIndex < 0xdd4)
        {
            return 2;
        }

        return overlayIndex - 0xdd1;
    }

    if (overlayIndex < 0xdf4)
    {
        if (overlayIndex < 0xde4)
        {
            return 0x10;
        }

        if (overlayIndex < 0xde7)
        {
            return 0x11;
        }
    }
    else if (overlayIndex > 0xdf7)
    {
        if (overlayIndex < 0xe07)
        {
            return 0x25;
        }

        if (overlayIndex < 0xe16)
        {
            return 0x26;
        }

        if (overlayIndex < 0xe25)
        {
            return 0x27;
        }

        if (overlayIndex < 0xe34)
        {
            return 0x28;
        }

        if (overlayIndex < 0xe42)
        {
            return overlayIndex - 0xe0b;
        }

        if (overlayIndex < 0xe51)
        {
            return 0x37;
        }

        if (overlayIndex < 0xe60)
        {
            return 0x38;
        }

        if (overlayIndex < 0xe6f)
        {
            return 0x39;
        }

        return overlayIndex > 0xe7d ? 0 : 0x3a;
    }

    return overlayIndex - 0xdd5;
}

namespace
{
    /// <summary>Side, in tiles, of the blocks an .elv file stores its tiles in.</summary>
    constexpr int32_t ELV_BLOCK_SIDE = 20;
    /// <summary>One .elv tile: byte 0 the elevation, then (at 4 and 6) the texture and overlay indices.</summary>
    constexpr int32_t ELV_TILE_SIZE = 8;
    /// <summary>Highest elevation level a tile can store above the map's base.</summary>
    constexpr uint32_t MAX_TILE_ELEVATION = 0x3f;
    /// <summary>Entries of the template mask cache (0x4e20 bytes).</summary>
    constexpr int32_t TEMPLATE_CACHE_SIZE = 5000;
    /// <summary>How many of them the editor sets to -1 (the rest it leaves as allocated; EB-029).</summary>
    constexpr int32_t TEMPLATE_CACHE_CLEARED = 0x4e2;
    /// <summary>A cache entry for an index with no template.</summary>
    constexpr int32_t NO_TEMPLATE = -2;
    /// <summary>A word or line of template.fit.</summary>
    constexpr int32_t TEMPLATE_WORD_SIZE = 0x100;
    /// <summary>Cells within this many meters of one of the map's four corner tips are blocked.</summary>
    constexpr double MAP_TIP_RADIUS = 1300.0;
    /// <summary>The tile type bits of MapTile::cells.</summary>
    constexpr uint32_t TILE_TYPE_MASK = 0x7f;
    /// <summary>MapTile::cells: bits 7-12 elevation, bit 13 "preserved".</summary>
    constexpr uint32_t TILE_ELEVATION_SHIFT = 7;
    constexpr uint32_t TILE_ELEVATION_MASK = 0x1f80;
    constexpr uint32_t TILE_PRESERVED = 0x2000;
    /// <summary>MapTile::overlay bit 24: an object stands on the tile.</summary>
    constexpr uint32_t TILE_HAS_OBJECT = 0x1000000;
    /// <summary>The game's object type numbers (ObjectTypeNum) placeTerrainObject knows.</summary>
    constexpr int32_t OBJECT_TYPE_TERRAIN = 1;
    constexpr int32_t OBJECT_TYPE_BUILDING = 0xb;
    constexpr int32_t OBJECT_TYPE_MINE = 0xd;
    constexpr int32_t OBJECT_TYPE_TREE = 0x12;
    constexpr int32_t OBJECT_TYPE_BRIDGE = 0x15;
    constexpr int32_t OBJECT_TYPE_GATE = 0x16;
    /// <summary>The overlay types a gate's tile gets, by its direction.</summary>
    constexpr uint32_t GATE_OVERLAY_A = 0x46;
    constexpr uint32_t GATE_OVERLAY_B = 0x44;

    /// <summary>Sets cell <paramref name="cell"/>'s passable bit of <paramref name="tile"/>.</summary>
    void SetCellPassable(MapTile& tile, int32_t cell, bool passable)
    {
        const uint32_t shift = static_cast<uint32_t>(cell * 2);
        tile.cells = (tile.cells & ~(0x4000u << shift)) | (static_cast<uint32_t>(passable) << (shift + 14));
    }

    /// <summary>Sets cell <paramref name="cell"/>'s line-of-sight bit of <paramref name="tile"/>.</summary>
    void SetCellLineOfSight(MapTile& tile, int32_t cell, bool lineOfSight)
    {
        const uint32_t shift = static_cast<uint32_t>(cell * 2);
        tile.cells = (tile.cells & ~(0x8000u << shift)) | (static_cast<uint32_t>(lineOfSight) << (shift + 15));
    }

    /// <summary>Sets every cell of <paramref name="tile"/> passable or not, seen through or not.</summary>
    void SetAllCells(MapTile& tile, bool passable, bool lineOfSight)
    {
        for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
        {
            SetCellPassable(tile, cell, passable);
            SetCellLineOfSight(tile, cell, lineOfSight);
        }
    }

    /// <summary>
    /// Applies a template mask: bit k set blocks cell k. With <paramref name="maskSight"/> the mask blocks sight too;
    /// else every cell is seen through.
    /// </summary>
    void ApplyCellMask(MapTile& tile, uint32_t mask, bool maskSight)
    {
        for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
        {
            const bool open = (mask & (1u << cell)) == 0;
            SetCellPassable(tile, cell, open);
            SetCellLineOfSight(tile, cell, maskSight ? open : true);
        }
    }

    /// <summary>Rewinds template.fit, clearing the end-of-file and failure marks of the last lookup.</summary>
    void RewindTemplate(std::ifstream& stream)
    {
        // EB-033 fixed: template.fit ends with "FITend", not the "FitEnd" the lookup stops at, so a texture without a
        // template reads to the end of the file. The editor's rewind left that stream failed, failing every later
        // lookup; clearing it first lets them find their templates.
        stream.clear();
        stream.seekg(0);
    }

    /// <summary>
    /// Looks "Tile<paramref name="index"/>" up in template.fit, from the start, reading words into
    /// <paramref name="word"/> (which keeps the last one between lookups), and reads the mask at the end of its line.
    /// </summary>
    /// <param name="scanIndex">Where the scan back over the mask's digits stopped (-1 when it ran off the line).</param>
    /// <returns>Whether the entry was found before "FitEnd" or the end of the file.</returns>
    bool LookUpTemplate(std::ifstream& stream, char (&word)[TEMPLATE_WORD_SIZE], int32_t index, int32_t* mask,
                        int32_t* scanIndex)
    {
        RewindTemplate(stream);
        char name[24];
        std::sprintf(name, "Tile%d", index);

        while (std::strcmp(word, name) != 0)
        {
            // Original behaviour (EB-019): the word is read without a limit; the port stops at the buffer's size.
            stream >> word;

            if (stream.fail() || stream.bad())
            {
                return false;
            }

            if (std::strcmp(word, "FitEnd") == 0)
            {
                return false;
            }
        }

        stream.getline(word, TEMPLATE_WORD_SIZE);
        int32_t i = static_cast<int32_t>(std::strlen(word)) - 1;

        while (i > -1 && std::isdigit(static_cast<unsigned char>(word[i])) != 0)
        {
            i--;
        }

        *mask = std::atoi(word + i + 1);
        *scanIndex = i;
        return true;
    }
}

int32_t ScenarioMap::buildFromTerrain(const char* elvName, EditorMap* editorMap, const char* dataPath, bool planet)
{
    CurPlanet = planet ? 1 : 0;

    if (map != nullptr)
    {
        std::memset(map, 0, static_cast<size_t>(height * width) * sizeof(MapTile));
    }

    CFile elvFile;

    if (!elvFile.Open(elvName, CFile::modeRead))
    {
        return 0;
    }

    // Two longs, the second the offset of the tile blocks.
    int32_t header = 0;
    elvFile.Read(&header, sizeof(header));
    elvFile.Read(&header, sizeof(header));
    elvFile.Seek(header, CFile::begin);
    height = editorMap->maxCol + 1;
    width = editorMap->maxRow + 1;
    // Original behaviour: init takes the width first, so the two swap (the editor's maps are square).
    init(height, width);
    const int32_t numTiles = height * width;
    // Original behaviour (EB-029): the early returns from here on leak the buffers.
    std::vector<uint8_t> tiles(static_cast<size_t>(numTiles) * ELV_TILE_SIZE);
    std::vector<uint8_t> block(static_cast<size_t>(ELV_BLOCK_SIDE * ELV_BLOCK_SIDE) * ELV_TILE_SIZE);
    const int32_t blocksPerSide = width / ELV_BLOCK_SIDE;

    for (int32_t b = 0; b < blocksPerSide * blocksPerSide; b++)
    {
        uint8_t* dest = tiles.data() + static_cast<size_t>(width * (b / blocksPerSide) * ELV_BLOCK_SIDE +
                                                           (b % blocksPerSide) * ELV_BLOCK_SIDE) *
                                           ELV_TILE_SIZE;

        if (elvFile.Read(block.data(), static_cast<UINT>(block.size())) == 0)
        {
            return 0;
        }

        for (int32_t row = 0; row < ELV_BLOCK_SIDE; row++)
        {
            std::memcpy(dest, block.data() + static_cast<size_t>(row * ELV_BLOCK_SIDE) * ELV_TILE_SIZE,
                        static_cast<size_t>(ELV_BLOCK_SIDE) * ELV_TILE_SIZE);
            dest += static_cast<size_t>(width) * ELV_TILE_SIZE;
        }
    }

    baseElevation = 0xff;

    for (int32_t i = 0; i < numTiles; i++)
    {
        if (tiles[static_cast<size_t>(i) * ELV_TILE_SIZE] < baseElevation)
        {
            baseElevation = tiles[static_cast<size_t>(i) * ELV_TILE_SIZE];
        }
    }

    // Texture tiles with blocked cells list them in teditor\template.fit: "Tile<index> ... <mask>".
    char templateName[256];
    std::strcpy(templateName, dataPath);
    std::strcat(templateName, "teditor\\template.fit");
    std::ifstream templateFile(templateName);

    if (templateFile.fail() || templateFile.bad())
    {
        CString format;
        format.LoadString(AFX_IDS_APP_ID);
        CString text;
        text.Format(format, templateName);
        CString caption;
        caption.LoadString(IDS_MCEDITOR);
        MessageBoxA(nullptr, text, caption, MB_OK);
        return 0;
    }

    // The masks found so far, by texture index (and NO_TEMPLATE by overlay index).
    // Original behaviour (EB-029): only the first quarter is set to -1, the rest left as allocated; the port zeroes
    // it, which reads like -1 (not looked up).
    std::vector<int32_t> templateCache(TEMPLATE_CACHE_SIZE, 0);
    std::fill_n(templateCache.begin(), TEMPLATE_CACHE_CLEARED, -1);
    const auto inCache = [](int32_t index) { return index >= 0 && index < TEMPLATE_CACHE_SIZE; };
    // The words last read by each kind of lookup (kept between lookups).
    char textureWord[TEMPLATE_WORD_SIZE] = {};
    char overlayWord[TEMPLATE_WORD_SIZE] = {};
    const auto elvTexture = [&tiles](int32_t i)
    {
        int16_t value;
        std::memcpy(&value, tiles.data() + static_cast<size_t>(i) * ELV_TILE_SIZE + 4, sizeof(value));
        return value;
    };

    for (int32_t i = 0; i < numTiles; i++)
    {
        const uint8_t* elvTile = tiles.data() + static_cast<size_t>(i) * ELV_TILE_SIZE;
        const int16_t texture = elvTexture(i);
        int16_t overlayIndex;
        std::memcpy(&overlayIndex, elvTile + 6, sizeof(overlayIndex));
        MapTile& tile = map[i];
        tile.cells = (tile.cells & ~TILE_TYPE_MASK) | static_cast<uint32_t>(calcTileTypeFromIndex(texture));
        tile.overlay =
            (tile.overlay & ~OVERLAY_TYPE_MASK) | static_cast<uint32_t>(calcOverlayTypeFromIndex(overlayIndex));
        uint32_t elevation = static_cast<uint32_t>(elvTile[0]) - static_cast<uint32_t>(baseElevation);

        if (elevation > MAX_TILE_ELEVATION)
        {
            elevation = MAX_TILE_ELEVATION;
        }

        tile.cells = (tile.cells & ~TILE_ELEVATION_MASK) | elevation << TILE_ELEVATION_SHIFT;
        tile.cells &= ~TILE_PRESERVED;
        SetAllCells(tile, true, true);
        int32_t mask = -1;

        if ((texture >= 0xc58 && texture < 0xd00) || (texture >= 0xd1e && texture < 0xd3e))
        {
            const int32_t cached = templateCache[texture];

            if (cached == NO_TEMPLATE)
            {
                SetAllCells(tile, false, true);
            }
            else if (cached > 0)
            {
                mask = cached;
            }
            else
            {
                int32_t scanIndex = 0;

                if (LookUpTemplate(templateFile, textureWord, texture, &mask, &scanIndex))
                {
                    // Original behaviour (EB-030): the mask is cached under the texture of the tile numbered by
                    // where the digit scan stopped, not under this tile's texture.
                    if (scanIndex >= 0 && inCache(elvTexture(scanIndex)))
                    {
                        templateCache[elvTexture(scanIndex)] = mask;
                    }
                }
                else
                {
                    mask = NO_TEMPLATE;
                    templateCache[texture] = NO_TEMPLATE;
                    SetAllCells(tile, false, true);
                }
            }

            if (templateCache[texture] != NO_TEMPLATE)
            {
                ApplyCellMask(tile, static_cast<uint32_t>(mask), false);
            }
        }

        // Original behaviour (EB-030): the overlay lookup also wants a negative index, so it never runs, and every
        // overlay index is cached as having no template: bridges block every cell and sight, and the rest of the
        // ranges below block passage.
        int32_t overlayMask = NO_TEMPLATE;
        const bool templateOverlay =
            (overlayIndex >= 0xd3e && overlayIndex <= 0xd51) || (overlayIndex >= 0xda8 && overlayIndex <= 0xdbc) ||
            (overlayIndex >= 0xd0a && overlayIndex <= 0xd0e) || (overlayIndex >= 0xd66 && overlayIndex <= 0xd6e);

        if (templateOverlay && overlayIndex < 0 && overlayIndex != NO_TEMPLATE)
        {
            int32_t found = 0;
            int32_t scanIndex = 0;

            if (LookUpTemplate(templateFile, overlayWord, overlayIndex, &found, &scanIndex))
            {
                overlayMask = found;
            }
        }

        // Original behaviour (EB-030): a negative overlay index writes before the cache; the port skips it.
        if (inCache(overlayIndex))
        {
            templateCache[overlayIndex] = overlayMask;
        }

        // Bridges.
        if (overlayIndex >= 0xda8 && overlayIndex < 0xdbc)
        {
            const int32_t cached = templateCache[overlayIndex];

            if (cached == NO_TEMPLATE)
            {
                SetAllCells(tile, false, false);
            }
            else
            {
                mask = cached;
                ApplyCellMask(tile, static_cast<uint32_t>(mask), true);
            }
        }

        if (overlayIndex >= 0xd3e && overlayIndex < 0xd51)
        {
            const int32_t cached = templateCache[overlayIndex];

            if (cached == NO_TEMPLATE)
            {
                SetAllCells(tile, false, true);
            }
            else
            {
                mask = cached;
                ApplyCellMask(tile, static_cast<uint32_t>(mask), false);
            }
        }

        if ((overlayIndex >= 0xdf8 && overlayIndex < 0xe07) || (overlayIndex >= 0xe16 && overlayIndex <= 0xe24))
        {
            SetAllCells(tile, true, true);
        }

        if (overlayIndex >= 0xe7e && overlayIndex < 0xe81)
        {
            SetAllCells(tile, false, true);
        }

        if ((overlayIndex >= 0xd0a && overlayIndex < 0xd0e) || (overlayIndex >= 0xd66 && overlayIndex < 0xd6e))
        {
            const int32_t cached = templateCache[overlayIndex];

            if (cached == NO_TEMPLATE)
            {
                SetAllCells(tile, false, false);
            }
            else
            {
                mask = cached;
                ApplyCellMask(tile, static_cast<uint32_t>(mask), true);
            }
        }

        // On planet 1 the dirt roads (overlay types 1-15) block passage.
        if (CurPlanet == 1)
        {
            const int32_t overlayType = calcOverlayTypeFromIndex(overlayIndex);

            if (overlayType > 0 && overlayType < 0x10)
            {
                for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
                {
                    SetCellPassable(tile, cell, false);
                }
            }
        }
    }

    // Clear the corners outside the map's diamond.
    const int32_t mapSide = height;
    const int32_t half = mapSide / 2;
    const auto clearTile = [this](int32_t row, int32_t col)
    {
        MapTile& tile = map[row * width + col];
        tile.cells &= ~TILE_TYPE_MASK;
        SetAllCells(tile, false, false);
    };

    for (int32_t row = 0, count = half; row < half; row++, count--)
    {
        for (int32_t col = 0; col < count; col++)
        {
            clearTile(row, col);
        }

        for (int32_t col = half + row; col < mapSide; col++)
        {
            clearTile(row, col);
        }
    }

    if (half < mapSide)
    {
        int32_t count = 2 * half - mapSide + 1;
        int32_t right = mapSide;

        for (int32_t row = half; row < mapSide; row++, count++)
        {
            right--;

            for (int32_t col = 0; col < count; col++)
            {
                clearTile(row, col);
            }

            for (int32_t col = right; col < mapSide; col++)
            {
                clearTile(row, col);
            }
        }
    }

    // Block the cells near the diamond's four tips.
    const float tip = static_cast<float>(static_cast<double>(static_cast<float>(mapSide) * 128.0f) * 0.5);
    const double plusTip = tip;
    const double minusTip = -static_cast<double>(tip);

    for (int32_t row = 0; row < mapSide; row++)
    {
        const double y = tileRowToWorldCoord[row];

        for (int32_t col = 0; col < mapSide; col++)
        {
            const double x = tileColToWorldCoord[col];

            if (std::sqrt((x - plusTip) * (x - plusTip) + y * y) <= MAP_TIP_RADIUS ||
                std::sqrt(x * x + (y - plusTip) * (y - plusTip)) <= MAP_TIP_RADIUS ||
                std::sqrt((x - minusTip) * (x - minusTip) + y * y) <= MAP_TIP_RADIUS ||
                std::sqrt(x * x + (y - minusTip) * (y - minusTip)) <= MAP_TIP_RADIUS)
            {
                SetAllCells(map[row * width + col], false, false);
            }
        }
    }

    // Stamp the placed objects' footprints (drop zones have none).
    for (const std::list<EditorObject*>* list : {&editorMap->objects, &editorMap->dropZones})
    {
        for (const EditorObject* object : *list)
        {
            const uint32_t id = object->id;

            if (objectIdClass(id) == OBJECT_CLASS_DROPZONE)
            {
                continue;
            }

            const uint32_t group = objectIdGroup(id);
            const uint32_t typeIndex = objectIdType(id);
            const uint32_t frameIndex = objectIdFrame(id);
            std::vector<ObjectGroup>& footprintGroups =
                isMoverClass(objectIdClass(id)) ? objectLibrary->moverGroups : objectLibrary->objectGroups;
            const ObjectFrame& footprintFrame = footprintGroups[group].types[typeIndex].frames[frameIndex];
            // Original behaviour (EB-028): the type's other data always come from the non-mover groups, read with a
            // mover's indices too; the port skips a mover whose indices fall outside them.
            std::vector<ObjectGroup>& groups = objectLibrary->objectGroups;

            if (group >= groups.size() || typeIndex >= groups[group].types.size() ||
                frameIndex >= groups[group].types[typeIndex].frames.size())
            {
                continue;
            }

            const ObjectType& type = groups[group].types[typeIndex];
            placeTerrainObject(type.objectTypeNum, footprintFrame.footprintLow, footprintFrame.footprintHigh,
                               object->cellCol, object->cellRow, type.blocksLineOfFire, type.frames[frameIndex].fitId,
                               type.mineType);
        }
    }

    return 1;
}

void ScenarioMap::placeTerrainObject(int32_t objectTypeNum, uint32_t footprintLow, uint32_t footprintHigh,
                                     int32_t cellCol, int32_t cellRow, bool blocksLineOfSight, int32_t fitId,
                                     int32_t mineType)
{
    // The footprint is 8x8 cells with the object's cell at (4, 4); find its top left tile and cell.
    const int32_t tileR = cellRow / MAPCELL_DIM;
    const int32_t tileC = cellCol / MAPCELL_DIM;
    int32_t startCellR = cellRow % MAPCELL_DIM - 4;
    int32_t startCellC = cellCol % MAPCELL_DIM - 4;
    int32_t startTileR = tileR;
    int32_t startTileC = tileC;

    if (startCellR < 0)
    {
        const int32_t tilesBack = static_cast<int32_t>((2u - static_cast<uint32_t>(startCellR)) / 3);
        startTileR = tileR - tilesBack;
        startCellR += tilesBack * MAPCELL_DIM;
    }

    if (startCellC < 0)
    {
        const int32_t tilesBack = static_cast<int32_t>((2u - static_cast<uint32_t>(startCellC)) / 3);
        startTileC = tileC - tilesBack;
        startCellC += tilesBack * MAPCELL_DIM;
    }

    // Original behaviour (EB-031): no bounds checks; an object near the map's edge stamps outside it.
    const auto stamp = [&](bool stampSight)
    {
        int32_t cellR = startCellR;
        int32_t row = startTileR;

        for (int32_t bitRow = 0; bitRow < 8; bitRow++)
        {
            int32_t cellC = startCellC;
            int32_t col = startTileC;

            for (int32_t bitCol = 0; bitCol < 8; bitCol++)
            {
                const int32_t bit = bitRow * 8 + bitCol;
                const uint32_t word = bit < 32 ? footprintLow : footprintHigh;

                if ((word & (1u << (bit & 31))) != 0)
                {
                    MapTile& tile = map[width * row + col];
                    SetCellPassable(tile, cellC + cellR * MAPCELL_DIM, false);

                    if (stampSight)
                    {
                        SetCellLineOfSight(tile, cellC + cellR * MAPCELL_DIM, false);
                    }
                }

                if (++cellC == MAPCELL_DIM)
                {
                    cellC = 0;
                    col++;
                }
            }

            if (++cellR == MAPCELL_DIM)
            {
                cellR = 0;
                row++;
            }
        }
    };

    MapTile& centre = map[width * tileR + tileC];
    const bool hasFootprint = footprintLow != 0 || footprintHigh != 0;
    // A gate's tile gets an overlay type by the gate's direction.
    const auto markGate = [&centre](int32_t id, bool lateGates)
    {
        switch (id)
        {
            case 0x284:
            case 0x287:
            {
                centre.overlay = (centre.overlay & 0xffffffc6) | GATE_OVERLAY_A;
                break;
            }
            case 0x285:
            case 0x286:
            {
                centre.overlay = (centre.overlay & 0xffffffc4) | GATE_OVERLAY_B;
                break;
            }
            case 0x2b2:
            case 0x2b3:
            {
                if (lateGates)
                {
                    centre.overlay = (centre.overlay & 0xffffffc6) | GATE_OVERLAY_A;
                }

                break;
            }
            case 0x2b4:
            case 0x2b5:
            {
                if (lateGates)
                {
                    centre.overlay = (centre.overlay & 0xffffffc4) | GATE_OVERLAY_B;
                }

                break;
            }
            default:
                break;
        }
    };

    switch (objectTypeNum)
    {
        case OBJECT_TYPE_TERRAIN:
        {
            if (!hasFootprint)
            {
                return;
            }

            centre.overlay |= TILE_HAS_OBJECT;
            stamp(blocksLineOfSight);
            break;
        }
        case OBJECT_TYPE_TREE:
        {
            if (hasFootprint)
            {
                stamp(blocksLineOfSight);
            }

            centre.overlay |= TILE_HAS_OBJECT;
            break;
        }
        case OBJECT_TYPE_BRIDGE:
        {
            if (hasFootprint)
            {
                stamp(blocksLineOfSight);
                centre.overlay |= TILE_HAS_OBJECT;
            }

            break;
        }
        case OBJECT_TYPE_BUILDING:
        {
            if (hasFootprint)
            {
                stamp(blocksLineOfSight);
                centre.overlay |= TILE_HAS_OBJECT;
                markGate(fitId, false);
            }

            break;
        }
        case OBJECT_TYPE_GATE:
        {
            // A gate blocks passage only, whatever its sight setting.
            if (hasFootprint)
            {
                stamp(false);
            }

            centre.overlay |= TILE_HAS_OBJECT;
            markGate(fitId, true);
            break;
        }
        case OBJECT_TYPE_MINE:
        {
            // Mine layouts: Inner Sphere in overlay bits 13-14, Clan in bits 11-12.
            // Original behaviour (EB-032): the values aren't masked, so mine types 4-10 spill into the path lock bits
            // and 14 and up past bit 12.
            int32_t layout = mineType;

            if (layout == 3 || layout == 1)
            {
                layout = 2;
            }
            else if (layout == 0xd || layout == 0xb)
            {
                layout = 0xc;
            }

            if (layout > 10)
            {
                centre.overlay = static_cast<uint32_t>(layout + 0x1ffff6) * 0x800 | (centre.overlay & 0xffffe7ff);
            }
            else
            {
                centre.overlay = (centre.overlay & 0xffff9fff) | static_cast<uint32_t>(layout) << 13;
            }

            break;
        }
        default:
            break;
    }
}

//---------------------------------------------------------------------------
// MovePath

MovePath::~MovePath()
{
    destroy();
}

int32_t MovePath::init(int32_t newNumSteps)
{
    numStepsWhenNotPaused = newNumSteps;
    numSteps = newNumSteps;

    if (maxMovePathSteps < newNumSteps)
    {
        maxMovePathSteps = newNumSteps;
        return newNumSteps;
    }

    return -1;
}

void MovePath::destroy()
{
    numSteps = 0;
}

//---------------------------------------------------------------------------
// GlobalMap

GlobalMap::GlobalMap()
{
    PathFindMap = nullptr;
    sectorDim = 12;
}

void GlobalMap::init(int32_t newWidth, int32_t newHeight)
{
    width = newWidth;
    height = newHeight;
    areaMap = AllocZeroed<int16_t>(static_cast<size_t>(newWidth * newHeight));

    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            areaMap[width * row + col] = AREA_NONE;
        }
    }

    numAreas = 0;
    areas = nullptr;
    numDoors = 0;
    doors = nullptr;
    doorBuildList = nullptr;
    pathCostTable = nullptr;
    sectorDim = 10;
    sectorWidth = newWidth / 10;
    sectorHeight = newWidth / 10;
}

int32_t GlobalMap::init(ScenarioMap* scenarioMap, int32_t newHeight, int32_t newWidth)
{
    GameMap = scenarioMap;

    if (newHeight == -1)
    {
        newHeight = scenarioMap->height;
    }

    if (newWidth == -1)
    {
        newWidth = scenarioMap->width;
    }

    init(newHeight, newWidth); // as the original: the map's height goes to init's width slot (maps are square)
    calcAreas(scenarioMap);
    calcBridges(scenarioMap);
    calcGlobalDoors(scenarioMap);
    // The original runs two empty loops here, counting down numAreas and numDoors.
    calcAreaDoors();
    calcDoorLinks();

    if (numAreas < SMALL_AREA_MAP_LIMIT)
    {
        smallAreaMap = AllocZeroed<uint8_t>(static_cast<size_t>(width * height));

        for (int32_t row = 0; row < height; row++)
        {
            for (int32_t col = 0; col < width; col++)
            {
                const int16_t area = areaMap[width * row + col];
                smallAreaMap[width * row + col] = area < 0 ? 0xff : static_cast<uint8_t>(area);
            }
        }

        std::free(areaMap);
        areaMap = nullptr;
    }

    return 0;
}

int32_t GlobalMap::write(CFile* mapFile)
{
    WriteValue(mapFile, GLOBALMAP_VERSION);
    WriteValue(mapFile, GLOBALMAP_HEADER_SPARE);
    WriteValue(mapFile, GLOBALMAP_HEADER_SPARE);
    WriteValue(mapFile, height);
    WriteValue(mapFile, width);
    WriteValue(mapFile, sectorDim);
    WriteValue(mapFile, sectorHeight);
    WriteValue(mapFile, sectorWidth);
    WriteValue(mapFile, numAreas);
    WriteValue(mapFile, numDoors);
    WriteValue(mapFile, numDoorInfos);
    WriteValue(mapFile, numDoorLinks);

    if (smallAreaMap != nullptr)
    {
        mapFile->Write(smallAreaMap, static_cast<UINT>(width * height));
    }
    else
    {
        mapFile->Write(areaMap, static_cast<UINT>(width * height * 2));
    }

    for (int32_t i = 0; i < numAreas; i++)
    {
        mapFile->Write(areas[i].doors, static_cast<UINT>(areas[i].numDoors * sizeof(DoorInfo)));
    }

    // Original behaviour (EB-017): the area and door arrays go out raw, heap pointers included; the rebuild writes 0
    // for the pointers.
    std::vector<uint8_t> records(static_cast<size_t>(numAreas) * GLOBALMAP_AREA_RECORD_SIZE);

    for (int32_t i = 0; i < numAreas; i++)
    {
        EncodeArea(areas[i], records.data() + static_cast<size_t>(i) * GLOBALMAP_AREA_RECORD_SIZE);
    }

    mapFile->Write(records.data(), static_cast<UINT>(records.size()));
    const int32_t totalDoors = numDoors + 2;

    for (int32_t i = 0; i < totalDoors; i++)
    {
        mapFile->Write(doors[i].links[0], static_cast<UINT>((doors[i].numLinks[0] + 2) * sizeof(DoorLink)));
        mapFile->Write(doors[i].links[1], static_cast<UINT>((doors[i].numLinks[1] + 2) * sizeof(DoorLink)));
    }

    records.assign(static_cast<size_t>(totalDoors) * GLOBALMAP_DOOR_RECORD_SIZE, 0);

    for (int32_t i = 0; i < totalDoors; i++)
    {
        EncodeDoor(doors[i], records.data() + static_cast<size_t>(i) * GLOBALMAP_DOOR_RECORD_SIZE);
    }

    mapFile->Write(records.data(), static_cast<UINT>(records.size()));
    calcPathCostTable();
    mapFile->Write(pathCostTable, static_cast<UINT>(numAreas * numAreas));
    return 0;
}

void GlobalMap::destroy()
{
    if (smallAreaMap != nullptr)
    {
        std::free(smallAreaMap);
        smallAreaMap = nullptr;
    }

    if (areaMap != nullptr)
    {
        std::free(areaMap);
        areaMap = nullptr;
    }

    if (areas != nullptr)
    {
        // Computed maps give each area its own door list; read ones point into doorInfos.
        if (doorInfos == nullptr)
        {
            for (int32_t i = 0; i < numAreas + 1; i++)
            {
                if (areas[i].doors != nullptr)
                {
                    std::free(areas[i].doors);
                    areas[i].doors = nullptr;
                }
            }
        }

        std::free(areas);
        areas = nullptr;
    }

    if (doors != nullptr)
    {
        if (doorLinks == nullptr)
        {
            for (int32_t i = 0; i < numDoors + 2; i++)
            {
                for (int32_t side = 0; side < 2; side++)
                {
                    if (doors[i].links[side] != nullptr)
                    {
                        std::free(doors[i].links[side]);
                        doors[i].links[side] = nullptr;
                    }
                }
            }
        }

        std::free(doors);
        doors = nullptr;
    }

    if (doorInfos != nullptr)
    {
        std::free(doorInfos);
        doorInfos = nullptr;
    }

    if (doorLinks != nullptr)
    {
        std::free(doorLinks);
        doorLinks = nullptr;
    }

    if (pathCostTable != nullptr)
    {
        std::free(pathCostTable);
        pathCostTable = nullptr;
    }

    // The path finder and the open list go with the global map (MoveMap::destroy's work, inlined).
    if (PathFindMap != nullptr)
    {
        MoveMap* pathFindMap = PathFindMap;

        if (pathFindMap->map != nullptr)
        {
            std::free(pathFindMap->map);
            pathFindMap->map = nullptr;
        }

        if (openList != nullptr)
        {
            PriorityQueue* queue = openList;
            queue->destroy();
            delete queue;
            openList = nullptr;
        }

        delete pathFindMap;
        PathFindMap = nullptr;
    }
}

int32_t GlobalMap::calcArea(int32_t tileR, int32_t tileC)
{
    if (smallAreaMap == nullptr)
    {
        if (areaMap != nullptr)
        {
            const int32_t area = areaMap[width * tileR + tileC];

            if (area >= 0)
            {
                return area;
            }
        }
    }
    else
    {
        const int32_t area = smallAreaMap[width * tileR + tileC];

        if (area != 0xff)
        {
            return area;
        }
    }

    return -1;
}

int GlobalMap::fillNorthSouthBridgeArea(ScenarioMap* scenarioMap, int32_t row, int32_t col, int32_t area)
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    areaMap[row * width + col] = static_cast<int16_t>(area);

    for (const int32_t next : {row - 1, row + 1})
    {
        if (InFillSector(next, col) && TileOverlay(scenarioMap, next, col) == 0x25 &&
            areaMap[next * width + col] == AREA_NONE)
        {
            fillNorthSouthBridgeArea(scenarioMap, next, col, area);
        }
    }

    return 1;
}

int GlobalMap::fillEastWestBridgeArea(ScenarioMap* scenarioMap, int32_t row, int32_t col, int32_t area)
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    areaMap[row * width + col] = static_cast<int16_t>(area);

    for (const int32_t next : {col + 1, col - 1})
    {
        if (InFillSector(row, next) && TileOverlay(scenarioMap, row, next) == 0x27 &&
            areaMap[row * width + next] == AREA_NONE)
        {
            fillEastWestBridgeArea(scenarioMap, row, next, area);
        }
    }

    return 1;
}

int GlobalMap::fillNorthSouthRailroadBridgeArea(ScenarioMap* scenarioMap, int32_t row, int32_t col, int32_t area)
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    areaMap[row * width + col] = static_cast<int16_t>(area);

    for (const int32_t next : {row - 1, row + 1})
    {
        if (InFillSector(next, col) && TileOverlay(scenarioMap, next, col) == 0x37 &&
            areaMap[next * width + col] == AREA_NONE)
        {
            fillNorthSouthRailroadBridgeArea(scenarioMap, next, col, area);
        }
    }

    return 1;
}

int GlobalMap::fillEastWestRailroadBridgeArea(ScenarioMap* scenarioMap, int32_t row, int32_t col, int32_t area)
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    areaMap[row * width + col] = static_cast<int16_t>(area);

    for (const int32_t next : {col + 1, col - 1})
    {
        if (InFillSector(row, next) && TileOverlay(scenarioMap, row, next) == 0x39 &&
            areaMap[row * width + next] == AREA_NONE)
        {
            fillEastWestRailroadBridgeArea(scenarioMap, row, next, area);
        }
    }

    return 1;
}

int isLRBlocked(MapTile* tile)
{
    // Per cell (row-major), whether it is impassable.
    uint32_t blocked[9];

    for (int32_t cell = 0; cell < 9; cell++)
    {
        blocked[cell] = (~tile->cells >> (14 + cell * 2)) & 1;
    }

    // A full row or a full column of blocked cells.
    for (int32_t i = 0; i < 3; i++)
    {
        if (blocked[i * 3] != 0 && blocked[i * 3 + 1] != 0 && blocked[i * 3 + 2] != 0)
        {
            return 1;
        }
    }

    for (int32_t i = 0; i < 3; i++)
    {
        if (blocked[i] != 0 && blocked[i + 3] != 0 && blocked[i + 6] != 0)
        {
            return 1;
        }
    }

    return 0;
}

int GlobalMap::fillArea(ScenarioMap* scenarioMap, int32_t row, int32_t col, int32_t area)
{
    if (!InFillSector(row, col))
    {
        return 0;
    }

    const MapTile* mapTile = &scenarioMap->map[scenarioMap->width * row + col];
    const uint32_t overlay = mapTile->overlay & OVERLAY_TYPE_MASK;

    if (overlay == 0x27 || overlay == 0x25 || overlay == 0x39 || overlay == 0x37)
    {
        return 0;
    }

    MapTile tile = *mapTile;
    const uint32_t terrain = tile.cells & 0x7f;
    bool open = false;

    if ((tile.cells & 0x55554000) != 0 && isLRBlocked(&tile) == 0 && overlay != 0x3e && terrain != 0x2a &&
        terrain != 0x29)
    {
        open = BlockWallTiles == 0 || overlay != 0x3c;
    }

    if (!open)
    {
        areaMap[width * row + col] = AREA_BLOCKED;
        return 0;
    }

    areaMap[width * row + col] = static_cast<int16_t>(area);

    for (int32_t dir = 0; dir < 4; dir++)
    {
        const int32_t nextRow = adjTile[dir][0] + row;
        const int32_t nextCol = adjTile[dir][1] + col;

        if (InFillSector(nextRow, nextCol) && areaMap[width * nextRow + nextCol] == AREA_NONE)
        {
            fillArea(scenarioMap, nextRow, nextCol, area);
        }
    }

    return 1;
}

void GlobalMap::calcSectorAreas(ScenarioMap* scenarioMap, int32_t sectorR, int32_t sectorC)
{
    minTileR = sectorDim * sectorR;
    maxTileR = sectorDim + minTileR;
    minTileC = sectorDim * sectorC;
    maxTileC = sectorDim + minTileC;

    for (int32_t row = minTileR; row < maxTileR; row++)
    {
        for (int32_t col = minTileC; col < maxTileC; col++)
        {
            if (areaMap[width * row + col] != AREA_NONE)
            {
                continue;
            }

            int filled;

            switch (TileOverlay(scenarioMap, row, col))
            {
                case 0x25:
                    filled = fillNorthSouthBridgeArea(scenarioMap, row, col, numAreas);
                    break;
                case 0x27:
                    filled = fillEastWestBridgeArea(scenarioMap, row, col, numAreas);
                    break;
                case 0x37:
                    filled = fillNorthSouthRailroadBridgeArea(scenarioMap, row, col, numAreas);
                    break;
                case 0x39:
                    filled = fillEastWestRailroadBridgeArea(scenarioMap, row, col, numAreas);
                    break;
                default:
                    filled = fillArea(scenarioMap, row, col, numAreas);
                    break;
            }

            if (filled != 0)
            {
                numAreas++;
            }
        }
    }
}

void GlobalMap::calcAreas(ScenarioMap* scenarioMap)
{
    for (int32_t sectorR = 0; sectorR < sectorHeight; sectorR++)
    {
        for (int32_t sectorC = 0; sectorC < sectorWidth; sectorC++)
        {
            calcSectorAreas(scenarioMap, sectorR, sectorC);
        }
    }

    // One spare area past the last.
    const int32_t count = numAreas + 1;
    areas = AllocZeroed<GlobalMapArea>(static_cast<size_t>(count));

    for (int32_t i = 0; i < count; i++)
    {
        GlobalMapArea& area = areas[i];
        area.type = 0;
        area.numDoors = 0;
        area.doors = nullptr;
        area.open = 1;
        area.closed = 0;
    }

    for (int32_t sectorR = 0; sectorR < sectorHeight; sectorR++)
    {
        for (int32_t sectorC = 0; sectorC < sectorWidth; sectorC++)
        {
            minTileR = sectorDim * sectorR;
            maxTileR = sectorDim + minTileR;
            minTileC = sectorDim * sectorC;
            maxTileC = sectorDim + minTileC;

            for (int32_t row = minTileR; row < maxTileR; row++)
            {
                for (int32_t col = minTileC; col < maxTileC; col++)
                {
                    const int16_t area = areaMap[width * row + col];

                    if (area >= 0)
                    {
                        areas[area].sectorR = static_cast<int16_t>(sectorR);
                        areas[area].sectorC = static_cast<int16_t>(sectorC);
                    }
                }
            }
        }
    }
}

void GlobalMap::calcBridges(ScenarioMap* scenarioMap)
{
    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            const uint32_t overlay = TileOverlay(scenarioMap, row, col);

            if (overlay == 0x25 || overlay == 0x37)
            {
                areas[areaMap[width * row + col]].type = 1;
            }
            else if (overlay == 0x27 || overlay == 0x39)
            {
                areas[areaMap[width * row + col]].type = 2;
            }
        }
    }
}

void GlobalMap::beginDoorProcessing()
{
    doorBuildList = AllocZeroed<GlobalMapDoor>(MAX_BUILD_DOORS);
}

void GlobalMap::addDoor(int32_t area1, int32_t area2, int32_t row, int32_t col, int32_t cellR, int32_t cellC,
                        int32_t length, int32_t direction)
{
    for (int32_t i = 0; i < numDoors; i++)
    {
        const GlobalMapDoor& door = doorBuildList[i];

        if (door.row == row && door.col == col && door.cellR == cellR && door.cellC == cellC && door.length == length &&
            door.direction[0] == direction)
        {
            return;
        }
    }

    // Port fix: the original writes past the build list once it holds MAX_BUILD_DOORS doors (EB-009). Two records
    // stay free for the temporary doors endDoorProcessing copies.
    if (numDoors >= MAX_BUILD_DOORS - 2)
    {
        return;
    }

    GlobalMapDoor& door = doorBuildList[numDoors];
    door.row = static_cast<int16_t>(row);
    door.col = static_cast<int16_t>(col);
    door.cellR = static_cast<uint8_t>(cellR);
    door.cellC = static_cast<uint8_t>(cellC);
    door.length = static_cast<char>(length);
    door.open = 1;
    door.area[0] = static_cast<int16_t>(area1);
    door.areaCost[0] = 1;
    door.direction[0] = static_cast<char>(direction);
    door.area[1] = static_cast<int16_t>(area2);
    door.areaCost[1] = 1;
    door.direction[1] = static_cast<char>((direction + 2) % 4);
    numDoors++;
}

void GlobalMap::endDoorProcessing()
{
    if (doorBuildList == nullptr)
    {
        return;
    }

    // The two temporary doors past the real ones come along: never-written build list records.
    const size_t count = static_cast<size_t>(numDoors + 2);
    doors = AllocZeroed<GlobalMapDoor>(count);
    std::memcpy(doors, doorBuildList, count * sizeof(GlobalMapDoor));
    std::free(doorBuildList);
    doorBuildList = nullptr;
}

int32_t GlobalMap::numAreaDoors(int32_t area)
{
    int32_t count = 0;

    for (int32_t i = 0; i < numDoors; i++)
    {
        if (doors[i].area[0] == area || doors[i].area[1] == area)
        {
            count++;
        }
    }

    return count;
}

void GlobalMap::getAreaDoors(int32_t area, DoorInfo* doorList)
{
    for (int32_t i = 0; i < numDoors; i++)
    {
        if (doors[i].area[0] == area || doors[i].area[1] == area)
        {
            doorList->doorIndex = static_cast<int16_t>(i);
            doorList->doorSide = doors[i].area[1] == area ? 1 : 0;
            doorList++;
        }
    }
}

void GlobalMap::calcGlobalDoors(ScenarioMap* scenarioMap)
{
    beginDoorProcessing();
    // The cells of the sector's tiles, each holding the area its neighbour (east or south) crosses into.
    std::vector<int16_t> cellMap(DOOR_CELL_MAP_SIDE * DOOR_CELL_MAP_SIDE);

    for (int32_t sectorR = 0; sectorR < sectorHeight; sectorR++)
    {
        for (int32_t sectorC = 0; sectorC < sectorWidth; sectorC++)
        {
            // Direction 1 looks east, 2 south (adjTile).
            for (int32_t dir = 1; dir < 3; dir++)
            {
                std::fill(cellMap.begin(), cellMap.end(), AREA_NONE);
                minTileR = sectorDim * sectorR;
                maxTileR = sectorDim + minTileR;
                minTileC = sectorDim * sectorC;
                maxTileC = sectorDim + minTileC;
                const int32_t minCellR = minTileR * MAPCELL_DIM;
                const int32_t maxCellR = maxTileR * MAPCELL_DIM;
                const int32_t minCellC = minTileC * MAPCELL_DIM;
                const int32_t maxCellC = maxTileC * MAPCELL_DIM;
                auto cell = [&](int32_t cellRow, int32_t cellCol) -> int16_t&
                { return cellMap[(cellRow - minCellR) * DOOR_CELL_MAP_SIDE + (cellCol - minCellC)]; };

                for (int32_t row = minTileR; row < maxTileR; row++)
                {
                    for (int32_t col = minTileC; col < maxTileC; col++)
                    {
                        const int32_t area = areaMap[width * row + col];

                        if (area < 0)
                        {
                            continue;
                        }

                        const int32_t nextRow = adjTile[dir][0] + row;
                        const int32_t nextCol = adjTile[dir][1] + col;

                        if (nextRow < 0 || nextRow >= height || nextCol < 0 || nextCol >= width)
                        {
                            continue;
                        }

                        const int16_t nextArea = areaMap[width * nextRow + nextCol];

                        if (nextArea < 0 || area == nextArea)
                        {
                            continue;
                        }

                        // Bridges only join areas along their own direction.
                        const int32_t type = areas[area].type;
                        const int32_t nextType = areas[nextArea].type;
                        const int32_t crossType = dir == 1 ? 2 : 1;

                        if ((type != 0 && type != crossType) || (nextType != 0 && nextType != crossType))
                        {
                            continue;
                        }

                        const MapTile tile = scenarioMap->map[scenarioMap->width * row + col];
                        const MapTile nextTile = scenarioMap->map[scenarioMap->width * nextRow + nextCol];
                        const int32_t baseRow = row * MAPCELL_DIM;
                        const int32_t baseCol = col * MAPCELL_DIM;

                        for (int32_t i = 0; i < MAPCELL_DIM; i++)
                        {
                            if (dir == 1)
                            {
                                if (TileCellPassable(tile, i, 2) != 0 && TileCellPassable(nextTile, i, 0) != 0)
                                {
                                    cell(baseRow + i, baseCol + 2) = nextArea;
                                }
                            }
                            else
                            {
                                if (TileCellPassable(tile, 2, i) != 0 && TileCellPassable(nextTile, 0, i) != 0)
                                {
                                    cell(baseRow + 2, baseCol + i) = nextArea;
                                }
                            }
                        }
                    }
                }

                if (dir == 1)
                {
                    // Runs down each cell column, from the east edge.
                    for (int32_t cellCol = maxCellC - 1; cellCol >= minCellC; cellCol--)
                    {
                        int32_t cellRow = minCellR;

                        while (cellRow < maxCellR)
                        {
                            const int16_t nextArea = cell(cellRow, cellCol);

                            if (nextArea < 0)
                            {
                                cellRow++;
                                continue;
                            }

                            const int32_t tileC = cellCol / 3;
                            const int32_t area = areaMap[(cellRow / 3) * width + tileC];
                            int32_t length = 0;

                            while (cellRow < maxCellR && areaMap[(cellRow / 3) * width + tileC] == area &&
                                   cell(cellRow, cellCol) == nextArea)
                            {
                                length++;
                                cellRow++;
                            }

                            addDoor(area, nextArea, (cellRow - length) / 3, tileC, (cellRow - length) % 3, cellCol % 3,
                                    length, 1);
                        }
                    }
                }
                else
                {
                    // Runs along each cell row, from the south edge.
                    for (int32_t cellRow = maxCellR - 1; cellRow >= minCellR; cellRow--)
                    {
                        int32_t cellCol = minCellC;

                        while (cellCol < maxCellC)
                        {
                            const int16_t nextArea = cell(cellRow, cellCol);

                            if (nextArea < 0)
                            {
                                cellCol++;
                                continue;
                            }

                            const int32_t tileR = cellRow / 3;
                            const int32_t area = areaMap[cellCol / 3 + width * tileR];
                            int32_t length = 0;

                            while (cellCol < maxCellC && areaMap[cellCol / 3 + width * tileR] == area &&
                                   cell(cellRow, cellCol) == nextArea)
                            {
                                length++;
                                cellCol++;
                            }

                            addDoor(area, nextArea, tileR, (cellCol - length) / 3, cellRow % 3, (cellCol - length) % 3,
                                    length, dir);
                        }
                    }
                }
            }
        }
    }

    endDoorProcessing();
}

void GlobalMap::calcAreaDoors()
{
    numDoorInfos = 0;

    for (int32_t i = 0; i < numAreas; i++)
    {
        GlobalMapArea& area = areas[i];
        area.numDoors = static_cast<char>(numAreaDoors(i));
        numDoorInfos += area.numDoors;

        if (area.numDoors == 0)
        {
            area.doors = nullptr;
            continue;
        }

        area.doors = AllocZeroed<DoorInfo>(static_cast<size_t>(area.numDoors));
        getAreaDoors(i, area.doors);
    }
}

int32_t GlobalMap::calcLinkCost(int32_t startDoor, int32_t thruArea, int32_t goalDoor)
{
    // The dirt roads' weights, at every move level: nothing on planet 1, else the built-in values.
    for (int32_t level = 0; level < NUM_MOVE_LEVELS; level++)
    {
        const size_t first = static_cast<size_t>(level * OVERLAY_WEIGHT_LEVEL_SIZE +
                                                 FIRST_DIRT_ROAD_OVERLAY * MAPCELL_DIM * MAPCELL_DIM);
        const size_t count = NUM_DIRT_ROAD_OVERLAYS * MAPCELL_DIM * MAPCELL_DIM;

        if (CurPlanet == 1)
        {
            std::fill_n(OverlayWeightTable.begin() + first, count, 0);
        }
        else
        {
            std::copy_n(DefaultOverlayWeights.begin() + first, count, OverlayWeightTable.begin() + first);
        }
    }

    // The middle cell of a door, on the side facing thruArea.
    auto doorCell = [this, thruArea](int32_t doorIndex, int32_t& cellRow, int32_t& cellCol) -> bool
    {
        const GlobalMapDoor& door = doors[doorIndex];

        if (door.area[0] != thruArea && door.area[1] != thruArea)
        {
            return false;
        }

        const int32_t side = door.area[1] == thruArea ? 1 : 0;
        int32_t rowOffset;
        int32_t colOffset;

        if (door.direction[0] == 1)
        {
            colOffset = side;
            rowOffset = door.length / 2;
        }
        else
        {
            colOffset = door.length / 2;
            rowOffset = side;
        }

        cellCol = door.col * 3 + door.cellC + colOffset;
        cellRow = door.cellR + door.row * 3 + rowOffset;
        return true;
    };

    int32_t startRow = 0;
    int32_t startCol = 0;

    if (!doorCell(startDoor, startRow, startCol))
    {
        return -1;
    }

    int32_t goalRow = 0;
    int32_t goalCol = 0;

    if (!doorCell(goalDoor, goalRow, goalCol))
    {
        return -2;
    }

    vector_3d goalPos;
    goalPos.x = CellColToWorldX(goalCol);
    goalPos.y = CellRowToWorldY(goalRow);
    goalPos.z = 0.0f;

    if (PathFindMap == nullptr)
    {
        PathFindMap = new MoveMap;
        const int32_t side = SimpleMovePathRange * 2 + 1;
        PathFindMap->init(side, side);
    }

    ClearBridgeTiles = 1;
    MovePath path;
    const int32_t ULr = areas[thruArea].sectorR * sectorDim;
    const int32_t ULc = areas[thruArea].sectorC * sectorDim;
    PathFindMap->setUp(GameMap, ULr, ULc, sectorDim, sectorDim, nullptr, startRow % 3 + (startRow / 3 - ULr) * 3,
                       startCol % 3 + (startCol / 3 - ULc) * 3, goalPos, goalRow % 3 + (goalRow / 3 - ULr) * 3,
                       goalCol % 3 + (goalCol / 3 - ULc) * 3, nullptr, LINK_MOVE_LEVEL, 0, LINK_NUM_OFFSETS, 0);
    int32_t goalCell[2] = {goalRow, goalCol};
    PathFindMap->calcPath(&path, nullptr, goalCell);
    ClearBridgeTiles = 0;

    if (path.numSteps == 0)
    {
        return NO_LINK_COST;
    }

    return path.cost;
}

void GlobalMap::calcDoorLinks()
{
    int32_t maxAreaDoors = 0;
    numDoorLinks = 0;

    for (int32_t doorIndex = 0; doorIndex < numDoors; doorIndex++)
    {
        GlobalMapDoor& door = doors[doorIndex];

        for (int32_t side = 0; side < 2; side++)
        {
            door.numLinks[side] = 0;
            door.links[side] = nullptr;
            const int32_t area = door.area[side];
            const int32_t areaDoors = areas[area].numDoors;
            door.numLinks[side] = static_cast<char>(areaDoors - 1);
            door.links[side] = AllocZeroed<DoorLink>(static_cast<size_t>(door.numLinks[side] + 2));
            numDoorLinks += door.numLinks[side] + 2;
            DoorLink* link = door.links[side];

            for (int32_t i = 0; i < areaDoors; i++)
            {
                const int16_t otherIndex = areas[area].doors[i].doorIndex;

                if (otherIndex == doorIndex)
                {
                    continue;
                }

                link->doorIndex = otherIndex;
                link->doorSide = doors[otherIndex].area[1] == area ? 1 : 0;
                link->cost = calcLinkCost(doorIndex, area, otherIndex);
                link++;
            }

            if (maxAreaDoors < areaDoors)
            {
                maxAreaDoors = areaDoors;
            }
        }
    }

    // The temporary start and goal doors link out of any area, so they get room for the most doors an area has.
    for (int32_t doorIndex = numDoors; doorIndex < numDoors + 2; doorIndex++)
    {
        GlobalMapDoor& door = doors[doorIndex];
        door.numLinks[0] = static_cast<char>(maxAreaDoors);
        numDoorLinks += door.numLinks[0] + 2;
        door.links[0] = AllocZeroed<DoorLink>(static_cast<size_t>(door.numLinks[0] + 2));
        door.numLinks[1] = 0;
        numDoorLinks += door.numLinks[1] + 2;
        door.links[1] = AllocZeroed<DoorLink>(static_cast<size_t>(door.numLinks[1] + 2));
    }
}

void GlobalMap::calcSectorPaths(ScenarioMap*, int32_t, int32_t)
{
}

void GlobalMap::calcPathCostTable()
{
    pathCostTable = AllocZeroed<uint8_t>(static_cast<size_t>(numAreas * numAreas));
    GlobalPathStep path[MAX_GLOBAL_PATH];

    for (int32_t startArea = 0; startArea < numAreas; startArea++)
    {
        for (int32_t goalArea = 0; goalArea < numAreas; goalArea++)
        {
            if (startArea == goalArea)
            {
                pathCostTable[numAreas * startArea + goalArea] = 0;
            }
            else
            {
                pathCostTable[startArea * numAreas + goalArea] =
                    static_cast<uint8_t>(calcPath(startArea, goalArea, path));
            }
        }
    }
}

namespace
{
    /// <summary>Makes a temporary door joining <paramref name="areaIndex"/> to itself (the start or goal door).</summary>
    /// <remarks>The field setup GlobalMap::setStartDoor and setGoalDoor share.</remarks>
    void InitTempDoor(GlobalMapDoor& door, int32_t areaIndex, char numLinks)
    {
        door.direction[0] = -1;
        door.direction[1] = -1;
        door.area[0] = static_cast<int16_t>(areaIndex);
        door.area[1] = static_cast<int16_t>(areaIndex);
        door.row = 0;
        door.col = 0;
        door.cellR = 0;
        door.cellC = 0;
        door.length = 0;
        door.open = 1;
        door.areaCost[0] = 1;
        door.areaCost[1] = 1;
        door.numLinks[1] = 0;
        door.numLinks[0] = numLinks;
    }
}

void GlobalMap::setStartDoor(int32_t startArea)
{
    GlobalMapDoor& startDoor = doors[numDoors];
    const GlobalMapArea& area = areas[startArea];
    InitTempDoor(startDoor, startArea, area.numDoors);
    startDoor.fromAreaIndex = 1;

    for (int32_t i = 0; i < startDoor.numLinks[0]; i++)
    {
        const DoorInfo& info = area.doors[i];
        DoorLink& link = startDoor.links[0][i];
        link.doorIndex = info.doorIndex;
        link.doorSide = info.doorSide;
        link.cost = 1;
        // Links the area's door back to the start door, in the spare room past its links.
        GlobalMapDoor& areaDoor = doors[info.doorIndex];
        const int32_t side = info.doorSide;
        DoorLink& backLink = areaDoor.links[side][areaDoor.numLinks[side]];
        backLink.doorIndex = static_cast<int16_t>(numDoors);
        backLink.doorSide = 0;
        backLink.cost = 1;
        areaDoor.numLinks[side]++;
    }
}

void GlobalMap::resetStartDoor(int32_t startArea)
{
    const GlobalMapDoor& startDoor = doors[numDoors];

    for (int32_t i = 0; i < startDoor.numLinks[0]; i++)
    {
        const DoorInfo& info = areas[startArea].doors[i];
        doors[info.doorIndex].numLinks[static_cast<int32_t>(info.doorSide)]--;
    }
}

void GlobalMap::setGoalDoor(int32_t goalArea)
{
    // The editor formats " GlobalMap.setGoalDoor: bad goalArea (%d of %d) " for a goal area out of range, and
    // carries on.
    GlobalMapDoor& goalDoor = doors[numDoors + 1];
    const GlobalMapArea& area = areas[goalArea];
    goalSectorR = area.sectorR;
    goalSectorC = area.sectorC;
    InitTempDoor(goalDoor, goalArea, area.numDoors);

    for (int32_t i = 0; i < goalDoor.numLinks[0]; i++)
    {
        const DoorInfo& info = area.doors[i];
        DoorLink& link = goalDoor.links[0][i];
        link.doorIndex = info.doorIndex;
        link.doorSide = info.doorSide;
        link.cost = 1;
        GlobalMapDoor& areaDoor = doors[info.doorIndex];
        const int32_t side = info.doorSide;
        DoorLink& backLink = areaDoor.links[side][areaDoor.numLinks[side]];
        backLink.doorIndex = static_cast<int16_t>(numDoors + 1);
        backLink.doorSide = 0;
        backLink.cost = 1;
        areaDoor.numLinks[side]++;
    }
}

void GlobalMap::resetGoalDoor(int32_t goalArea)
{
    const GlobalMapDoor& goalDoor = doors[numDoors + 1];

    for (int32_t i = 0; i < goalDoor.numLinks[0]; i++)
    {
        const DoorInfo& info = areas[goalArea].doors[i];
        doors[info.doorIndex].numLinks[static_cast<int32_t>(info.doorSide)]--;
    }
}

int32_t GlobalMap::calcHPrime(int32_t door)
{
    const GlobalMapArea& area0 = areas[doors[door].area[0]];
    const GlobalMapArea& area1 = areas[doors[door].area[1]];
    const int32_t sectorR = (area0.sectorR + area1.sectorR) / 2;
    const int32_t sectorC = (area0.sectorC + area1.sectorC) / 2;
    const int32_t rowDistance = goalSectorR < sectorR ? sectorR - goalSectorR : goalSectorR - sectorR;
    const int32_t colDistance = goalSectorC < sectorC ? sectorC - goalSectorC : goalSectorC - sectorC;
    return rowDistance + colDistance;
}

int32_t GlobalMap::calcPath(int32_t startArea, int32_t goalArea, GlobalPathStep* path)
{
    if (startArea == -1 || goalArea == -1)
    {
        return -1;
    }

    CreateOpenList();
    const int32_t startDoor = numDoors;
    const int32_t goalDoor = numDoors + 1;

    for (int32_t i = 0; i < numDoors + 2; i++)
    {
        GlobalMapDoor& door = doors[i];
        door.cost = 1;
        door.parent = -1;
        door.fromAreaIndex = -1;
        door.flags = 0;
        door.g = 0;
        door.hPrime = -1;
        door.fPrime = 0;
    }

    setStartDoor(startArea);
    setGoalDoor(goalArea);
    PQNode startNode = {};
    startNode.key = 0;
    startNode.id = startDoor;
    openList->clear();
    openList->insert(startNode);
    doors[startDoor].flags |= ON_OPEN_LIST;
    bool goalFound = false;

    while (openList->size() != 0)
    {
        PQNode best;
        openList->remove(best);
        const int32_t curIndex = best.id;
        GlobalMapDoor& current = doors[curIndex];
        const int32_t g = current.g;
        current.flags = (current.flags & ~ON_OPEN_LIST) | ON_CLOSED_LIST;

        if (curIndex == goalDoor)
        {
            goalFound = true;
            break;
        }

        const int32_t side = 1 - current.fromAreaIndex;
        const int32_t thruArea = current.area[side];
        const int32_t numLinks = current.numLinks[side];

        for (int32_t i = 0; i < numLinks; i++)
        {
            const DoorLink& link = current.links[side][i];
            const int32_t succIndex = link.doorIndex;
            const int32_t linkCost = link.cost;
            GlobalMapDoor& successor = doors[succIndex];

            if (successor.open == 0 || linkCost >= BLOCKED_CELL_COST)
            {
                continue;
            }

            if (successor.hPrime == -1)
            {
                successor.hPrime = calcHPrime(succIndex);
            }

            const int32_t newG = g + linkCost;
            const int32_t succSide = successor.area[1] == thruArea ? 1 : 0;

            if ((successor.flags & ON_OPEN_LIST) == 0)
            {
                if ((successor.flags & ON_CLOSED_LIST) == 0)
                {
                    successor.fromAreaIndex = succSide;
                    successor.g = newG;
                    successor.parent = curIndex;
                    successor.fPrime = newG + successor.hPrime;
                    successor.cost = linkCost;
                    PQNode node = {};
                    node.key = successor.fPrime;
                    node.id = succIndex;
                    openList->insert(node);
                    successor.flags |= ON_OPEN_LIST;
                }
                else if (newG < successor.g)
                {
                    // A cheaper way to a closed door: reparent it and push the saving on.
                    successor.cost = linkCost;
                    successor.parent = curIndex;
                    successor.fromAreaIndex = succSide;
                    propogateCost(succIndex, linkCost, succSide, g);
                }
            }
            else if (newG < successor.g)
            {
                successor.fromAreaIndex = succSide;
                successor.cost = linkCost;
                successor.parent = curIndex;
                successor.g = newG;
                successor.fPrime = successor.hPrime + newG;
                // Original behaviour (EB-010): when the door isn't found, the editor formats "GlobalMap.calcPath:
                // Cannot find globalmap door ..." and still changes slot 0, the sentinel.
                openList->change(openList->find(succIndex), successor.fPrime);
            }
        }
    }

    resetStartDoor(startArea);
    resetGoalDoor(goalArea);

    if (!goalFound)
    {
        return 0;
    }

    int32_t count = 1;

    for (int32_t door = goalDoor; door != startDoor; door = doors[door].parent)
    {
        count++;
    }

    const int32_t numSteps = count - 1;
    int32_t costToGoal = 0;
    int32_t door = goalDoor;

    for (int32_t i = numSteps - 1; i >= 0; i--)
    {
        // Port fix: the original writes past the caller's MAX_GLOBAL_PATH steps on a longer path (EB-014).
        if (i < MAX_GLOBAL_PATH)
        {
            path[i].thruArea = doors[door].area[doors[door].fromAreaIndex];
            path[i].goalDoor = door;
            path[i].costToGoal = costToGoal;
        }

        costToGoal += doors[door].cost;
        door = doors[door].parent;
    }

    if (pathCostTable != nullptr)
    {
        uint8_t& entry = pathCostTable[numAreas * startArea + goalArea];

        if (entry != numSteps)
        {
            entry = count > 0xff ? 0xff : static_cast<uint8_t>(numSteps);
        }
    }

    return numSteps;
}

void GlobalMap::propogateCost(int32_t door, int32_t cost, int32_t fromSide, int32_t g)
{
    const int32_t newG = cost + g;
    GlobalMapDoor& current = doors[door];

    if (newG >= current.g)
    {
        return;
    }

    current.g = newG;
    current.fPrime = current.hPrime + newG;

    if ((current.flags & ON_OPEN_LIST) != 0)
    {
        // Original behaviour (EB-011): passes the door number where PriorityQueue::change wants the slot; a door that
        // isn't found only gets a "GlobalMap.propogateCost: Cannot find ..." message formatted.
        openList->find(door);
        openList->change(door, current.fPrime);
        return;
    }

    const int32_t side = 1 - fromSide;
    const int32_t numLinks = current.numLinks[side];

    for (int32_t i = 0; i < numLinks; i++)
    {
        const DoorLink& link = current.links[side][i];
        const int32_t nextIndex = link.doorIndex;
        const int32_t linkCost = link.cost;
        GlobalMapDoor& next = doors[nextIndex];
        const int32_t nextSide = next.area[1] == current.area[side] ? 1 : 0;

        if (next.open == 0 || linkCost >= BLOCKED_CELL_COST || next.hPrime == -1)
        {
            continue;
        }

        if (door == next.parent)
        {
            // Original behaviour (EB-012): passes this door's exit side, not the next door's entry side.
            propogateCost(nextIndex, linkCost, side, current.g);
        }
        else if (current.g + linkCost < next.g)
        {
            next.cost = linkCost;
            next.parent = door;
            next.fromAreaIndex = nextSide;
            propogateCost(nextIndex, linkCost, nextSide, current.g);
        }
    }
}

int32_t GlobalMap::getPathCost(int32_t startArea, int32_t goalArea)
{
    if (startArea < 0 || goalArea < 0)
    {
        return 0;
    }

    return pathCostTable[numAreas * startArea + goalArea];
}

//---------------------------------------------------------------------------
// MoveMap

void MoveMap::init(int32_t newMaxWidth, int32_t newMaxHeight)
{
    maxWidth = newMaxWidth;
    width = newMaxWidth;
    maxHeight = newMaxHeight;
    maxCellHeight = newMaxHeight * MAPCELL_DIM;
    cellHeight = newMaxHeight * MAPCELL_DIM;
    height = newMaxHeight;
    maxCellWidth = newMaxWidth * MAPCELL_DIM;
    cellWidth = newMaxWidth * MAPCELL_DIM;
    map = AllocZeroed<MoveMapNode>(static_cast<size_t>(maxCellHeight * maxCellWidth));
    clear();
}

void MoveMap::clear()
{
    const int32_t numCells = cellHeight * maxCellWidth;

    for (int32_t i = 0; i < numCells; i++)
    {
        map[i].parent = -1;
        map[i].flags = 0;
        map[i].hPrime = -1;
    }

    goalPos.zero();
    target = {NO_POSITION, NO_POSITION, NO_POSITION};
}

void MoveMap::placeMovers(int)
{
}

void MoveMap::setStart(vector_3d* newStartPos, int32_t newStartR, int32_t newStartC)
{
    if (newStartPos == nullptr)
    {
        startPos = {NO_POSITION, NO_POSITION, NO_POSITION};
    }
    else
    {
        startPos = *newStartPos;
    }

    if (newStartR == -1)
    {
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        ScenarioMap::worldToMapPos(*newStartPos, tileR, tileC, cellR, cellC);
        startR = cellR + (tileR - ULr) * 3;
        startC = cellC + (tileC - ULc) * 3;
        return;
    }

    startR = newStartR;
    startC = newStartC;
}

void MoveMap::setGoal(vector_3d newGoalPos, int32_t newGoalR, int32_t newGoalC)
{
    goalPos = newGoalPos;

    if (newGoalR == -1)
    {
        int32_t tileR = 0;
        int32_t tileC = 0;
        int32_t cellR = 0;
        int32_t cellC = 0;
        ScenarioMap::worldToMapPos(goalPos, tileR, tileC, cellR, cellC);
        doorDirection = -1;
        goalR = cellR + (tileR - ULr) * 3;
        goalC = cellC + (tileC - ULc) * 3;
        GoalIsDoor = 0;
        return;
    }

    goalR = newGoalR;
    goalC = newGoalC;
    doorDirection = -1;
    GoalIsDoor = 0;
}

int32_t MoveMap::setUp(ScenarioMap* scenarioMap, int32_t newULr, int32_t newULc, int32_t newHeight, int32_t newWidth,
                       vector_3d* newStartPos, int32_t newStartR, int32_t newStartC, vector_3d newGoalPos,
                       int32_t newGoalR, int32_t newGoalC, int32_t* newOverlayWeightTable, int32_t newMoveLevel,
                       int32_t newJumpCost, int32_t newNumOffsets, uint32_t params)
{
    if (map == nullptr)
    {
        init(newHeight, newWidth); // as the original: the height goes to init's width slot (windows are square)
    }
    else
    {
        width = newWidth;
        height = newHeight;
        cellHeight = newHeight * 3;
        cellWidth = newWidth * 3;
        clear();
    }

    ULr = newULr;
    ULc = newULc;
    minRow = newULr * 3;
    minCol = newULc * 3;
    overlayWeightTable = newOverlayWeightTable == nullptr ? OverlayWeightTable.data() : newOverlayWeightTable;
    jumpCost = newJumpCost;
    moveLevel = newMoveLevel;
    numOffsets = newNumOffsets;
    setStart(newStartPos, newStartR, newStartC);
    setGoal(newGoalPos, newGoalR, newGoalC);

    if (ClearBridgeTiles != 0)
    {
        AdjustBridgeWeights(overlayWeightTable, -BLOCKED_CELL_COST);
    }

    const int32_t lockCost = moveLevel * 8;

    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            const int32_t tileR = ULr + row;
            const int32_t tileC = ULc + col;

            if (tileR < 0 || tileR >= GameMap->height || tileC < 0 || tileC >= GameMap->width)
            {
                continue;
            }

            const MapTile tile = scenarioMap->map[scenarioMap->width * tileR + tileC];
            MoveMapNode* tileNodes = &map[maxCellWidth * row * 3 + col * 3];
            auto node = [&](int32_t cell) -> MoveMapNode& { return tileNodes[maxCellWidth * (cell / 3) + cell % 3]; };

            for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
            {
                node(cell).cost = TileCellPassable(tile, cell / 3, cell % 3) != 0 ? moveLevel : BLOCKED_CELL_COST;
            }

            const uint32_t overlay = tile.overlay & OVERLAY_TYPE_MASK;

            if (overlay != 0)
            {
                if (overlay < FIRST_GATE_OVERLAY || overlay > LAST_GATE_OVERLAY)
                {
                    const int32_t weights = OverlayWeightIndex[overlay];

                    for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
                    {
                        node(cell).cost = AddCost(node(cell).cost, overlayWeightTable[weights + cell]);
                    }
                }
                else
                {
                    const int32_t gateOverlay =
                        GateOverlayForAlignment[EDITOR_GATE_ALIGNMENT + 1][overlay - FIRST_GATE_OVERLAY];

                    if (gateOverlay == -1)
                    {
                        for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
                        {
                            node(cell).cost = AddCost(node(cell).cost, CLOSED_GATE_COST);
                        }
                    }
                    else
                    {
                        const int32_t weights = OverlayWeightIndex[gateOverlay];

                        for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++)
                        {
                            node(cell).cost = AddCost(node(cell).cost, overlayWeightTable[weights + cell]);
                        }
                    }
                }
            }

            uint32_t locks = (tile.overlay >> OVERLAY_LOCK_SHIFT) & 0x1ff;

            if (locks != 0 && (params & PARAM_USE_LOCKS) != 0)
            {
                for (int32_t cell = 0; cell < MAPCELL_DIM * MAPCELL_DIM; cell++, locks >>= 1)
                {
                    if ((locks & 1) != 0)
                    {
                        node(cell).cost = AddCost(node(cell).cost, lockCost);
                    }
                }
            }
        }
    }

    if (FindingEscapePath == 0)
    {
        // Port fix: the original marks a goal outside the window too, writing outside map (EB-016).
        if (goalR >= 0 && goalR < cellHeight && goalC >= 0 && goalC < cellWidth)
        {
            map[goalR * maxCellWidth + goalC].flags |= CELL_GOAL;
        }

        return 0;
    }

    // Every cell of a tile in the goal's area, or in an area with a path to it, is a goal.
    int32_t goalTileR = 0;
    int32_t goalTileC = 0;
    int32_t goalCellR = 0;
    int32_t goalCellC = 0;
    ScenarioMap::worldToMapPos(newGoalPos, goalTileR, goalTileC, goalCellR, goalCellC);
    const int32_t goalArea = GlobalMoveMap->calcArea(goalTileR, goalTileC);

    for (int32_t row = 0; row < height; row++)
    {
        for (int32_t col = 0; col < width; col++)
        {
            const int32_t tileR = ULr + row;
            const int32_t tileC = ULc + col;

            if (GameMap->inBounds(tileR, tileC) == 0)
            {
                continue;
            }

            const int32_t area = GlobalMoveMap->calcArea(tileR, tileC);
            const int32_t cost = GlobalMoveMap->getPathCost(area, goalArea);

            if (area != goalArea && cost <= 0)
            {
                continue;
            }

            for (int32_t cellR = 0; cellR < MAPCELL_DIM; cellR++)
            {
                for (int32_t cellC = 0; cellC < MAPCELL_DIM; cellC++)
                {
                    map[maxCellWidth * (row * 3 + cellR) + col * 3 + cellC].flags |= CELL_GOAL;
                }
            }
        }
    }

    return 0;
}

int MoveMap::adjacentCellOpen(int32_t r, int32_t c, int32_t dir)
{
    const int32_t nextR = cellShift[dir * 2] + r;
    const int32_t nextC = cellShift[dir * 2 + 1] + c;

    if (nextR < 0 || nextR >= cellHeight || nextC < 0 || nextC >= cellWidth)
    {
        return 0;
    }

    const MoveMapNode& node = map[maxCellWidth * nextR + nextC];

    if ((node.flags & CELL_HAS_MOVER) != 0)
    {
        return 0;
    }

    return node.cost < BLOCKED_CELL_COST ? 1 : 0;
}

void MoveMap::propogateCost(int32_t r, int32_t c, int32_t cost, int32_t g)
{
    MoveMapNode& current = map[maxCellWidth * r + c];
    const int32_t newG = g + cost;

    if (newG >= current.g)
    {
        return;
    }

    // Original behaviour (EB-013): the cell's own cost is replaced by the step cost (diagonal and jump extras
    // included), so later searches through it see the inflated cost.
    current.cost = cost;
    current.g = newG;
    current.fPrime = current.hPrime + newG;

    if ((current.flags & ON_OPEN_LIST) != 0)
    {
        // Original behaviour (EB-010): when the cell isn't found, the editor formats "MoveMap.propogateCost: Cannot
        // find movemap node ..." and still changes slot 0, the sentinel.
        openList->change(openList->find(CellId(r, c)), current.fPrime);
        return;
    }

    for (int32_t i = 0; i < numOffsets; i++)
    {
        if (IsDiagonalStep[i] != 0 && adjacentCellOpen(r, c, StepAdjDir[i]) == 0 &&
            adjacentCellOpen(r, c, StepAdjDir[i + 1]) == 0)
        {
            continue;
        }

        const int32_t nextR = r + cellShift[i * 2];
        const int32_t nextC = c + cellShift[i * 2 + 1];

        if (nextR < 0 || nextR >= cellHeight || nextC < 0 || nextC >= cellWidth)
        {
            continue;
        }

        MoveMapNode& next = map[maxCellWidth * nextR + nextC];

        if (next.cost >= BLOCKED_CELL_COST || next.hPrime == -1 || next.hPrime >= MaxHPrime)
        {
            continue;
        }

        const int32_t dir = reverseShift[i];
        const int32_t stepCost = StepCost(next.cost, i, jumpCost);

        if (dir != next.parent)
        {
            if (next.g <= current.g + stepCost)
            {
                continue;
            }

            next.parent = dir;
        }

        propogateCost(nextR, nextC, stepCost, current.g);
    }
}

int32_t MoveMap::calcPath(MovePath* path, vector_3d* goalWorldPos, int32_t* goalCell)
{
    const int32_t distance = std::abs(startR - goalR) + std::abs(startC - goalC);
    MaxHPrime = static_cast<int16_t>(static_cast<int32_t>(distance * 2.5));

    if (MaxHPrime < 500)
    {
        MaxHPrime = 500;
    }

    CreateOpenList();
    MoveMapNode& start = map[maxCellWidth * startR + startC];
    start.g = 0;
    const int32_t startH = std::abs(goalR - startR) + std::abs(goalC - startC);
    start.hPrime = startH;
    start.fPrime = startH;
    PQNode startNode;
    startNode.key = startH;
    startNode.id = CellId(startR, startC);
    startNode.row = startR;
    startNode.col = startC;
    openList->clear();
    openList->insert(startNode);
    start.flags |= ON_OPEN_LIST;
    int32_t bestR = -1;
    int32_t bestC = -1;
    bool goalFound = false;

    while (openList->size() != 0)
    {
        PQNode best;
        openList->remove(best);
        bestR = best.row;
        bestC = best.col;
        MoveMapNode& current = map[maxCellWidth * bestR + bestC];
        const int32_t g = current.g;
        const uint32_t flags = current.flags;
        current.flags = (flags & ~ON_OPEN_LIST) | ON_CLOSED_LIST;

        if ((flags & CELL_GOAL) != 0)
        {
            goalFound = true;
            break;
        }

        for (int32_t i = 0; i < numOffsets; i++)
        {
            if (IsDiagonalStep[i] != 0 && adjacentCellOpen(bestR, bestC, StepAdjDir[i]) == 0 &&
                adjacentCellOpen(bestR, bestC, StepAdjDir[i + 1]) == 0)
            {
                continue;
            }

            const int32_t nextR = cellShift[i * 2] + bestR;
            const int32_t nextC = cellShift[i * 2 + 1] + bestC;

            if (nextR < 0 || nextR >= cellHeight || nextC < 0 || nextC >= cellWidth)
            {
                continue;
            }

            MoveMapNode& next = map[maxCellWidth * nextR + nextC];

            if (next.cost >= BLOCKED_CELL_COST)
            {
                continue;
            }

            if (next.hPrime == -1)
            {
                next.hPrime = std::abs(goalR - nextR) + std::abs(goalC - nextC);
            }

            if (next.hPrime >= MaxHPrime)
            {
                continue;
            }

            const int32_t dir = reverseShift[i];
            const int32_t stepCost = StepCost(next.cost, i, jumpCost);
            const int32_t newG = stepCost + g;

            if ((next.flags & ON_OPEN_LIST) == 0)
            {
                if ((next.flags & ON_CLOSED_LIST) == 0)
                {
                    next.parent = dir;
                    next.g = newG;
                    next.fPrime = newG + next.hPrime;
                    PQNode node;
                    node.key = next.fPrime;
                    node.id = CellId(nextR, nextC);
                    node.row = nextR;
                    node.col = nextC;
                    openList->insert(node);
                    next.flags |= ON_OPEN_LIST;
                }
                else if (newG < next.g)
                {
                    next.parent = dir;
                    propogateCost(nextR, nextC, stepCost, g);
                }
            }
            else if (newG < next.g)
            {
                next.parent = dir;
                next.g = newG;
                next.fPrime = next.hPrime + newG;
                // Original behaviour (EB-010): when the cell isn't found, the editor formats "MoveMap.calcPath: Cannot
                // find movemap node ..." and still changes slot 0, the sentinel.
                openList->change(openList->find(CellId(nextR, nextC)), next.fPrime);
            }
        }
    }

    if (ClearBridgeTiles != 0)
    {
        AdjustBridgeWeights(overlayWeightTable, BLOCKED_CELL_COST);
    }

    if (!goalFound)
    {
        return 0;
    }

    goalCell[0] = bestR;
    goalCell[1] = bestC;
    int32_t count = 0;

    for (int32_t r = bestR, c = bestC; r != startR || c != startC;)
    {
        count++;
        const int32_t parent = map[maxCellWidth * r + c].parent;
        r += cellShift[parent * 2];
        c += cellShift[parent * 2 + 1];
    }

    if (doorDirection != -1)
    {
        count++;
    }

    path->goal.zero();
    path->numSteps = 0;
    path->numStepsWhenNotPaused = 0;
    path->curStep = 0;
    path->cost = 0;
    path->marked = 0;
    path->globalStep = -1;

    // Port fix: the original writes past stepList when the path is longer than MAX_STEPS_PER_MOVEPATH (EB-015).
    if (count == 0 || count > MAX_STEPS_PER_MOVEPATH)
    {
        return path->numSteps;
    }

    path->init(count);
    path->target = target;
    path->cost = map[maxCellWidth * bestR + bestC].g;
    int32_t stepIndex = count;

    if (doorDirection == -1)
    {
        if (goalWorldPos == nullptr)
        {
            path->goal = goalPos;
        }
        else
        {
            goalWorldPos->x = CellColToWorldX(minCol + bestC);
            goalWorldPos->y = CellRowToWorldY(minRow + bestR);
            goalWorldPos->z = 0.0f;
            path->goal = *goalWorldPos;
        }
    }
    else
    {
        // The last step goes through the door, into the next area.
        stepIndex = count - 1;
        PathStep& doorStep = path->stepList[stepIndex];
        doorStep.direction = static_cast<uint8_t>(static_cast<char>(doorDirection) << 1);
        const int32_t doorRow = minRow + adjTile[doorDirection][0] + bestR;
        const int32_t doorCol = adjTile[doorDirection][1] + bestC + minCol;
        goalCell[0] = doorRow;
        goalCell[1] = doorCol;
        const float x = CellColToWorldX(doorCol);
        const float y = CellRowToWorldY(doorRow);
        doorStep.destination = {x, y, 0.0f};
        doorStep.distanceToGoal = 0.0f;
        doorStep.tileR = static_cast<int16_t>(doorRow / 3);
        doorStep.tileC = static_cast<int16_t>(doorCol / 3);
        doorStep.cellR = static_cast<int16_t>(doorRow - doorStep.tileR * 3);
        doorStep.cellC = static_cast<int16_t>(doorCol - doorStep.tileC * 3);
        path->goal = {x, y, 0.0f};

        if (goalWorldPos != nullptr)
        {
            *goalWorldPos = {x, y, 0.0f};
        }
    }

    if (cellShiftDistanceReady == 0)
    {
        const float scale = metersPerWorldUnit * metersPerVertex * (1.0f / 3.0f);

        for (int32_t i = 0; i < NUM_CELL_OFFSETS; i++)
        {
            const float dr = static_cast<float>(cellShift[i * 2]);
            const float dc = static_cast<float>(cellShift[i * 2 + 1]);
            cellShiftDistance[i] = std::sqrt(dc * dc + dr * dr) * scale;
        }

        cellShiftDistanceReady = 1;
    }

    // Walks back from the goal cell, filling the steps from the end.
    for (int32_t r = bestR, c = bestC; r != startR || c != startC;)
    {
        stepIndex--;
        MoveMapNode& node = map[maxCellWidth * r + c];
        PathStep& step = path->stepList[stepIndex];
        step.direction = static_cast<uint8_t>(reverseShift[node.parent]);
        const int32_t mapRow = minRow + r;
        const int32_t mapCol = minCol + c;

        if (stepIndex == count - 1 && static_cast<int8_t>(step.direction) < 8)
        {
            step.distanceToGoal = 0.0f;
        }
        else
        {
            // As the original: for a final jump step this reads the (unused) step past the end.
            const PathStep& nextStep = path->stepList[stepIndex + 1];
            step.distanceToGoal = nextStep.distanceToGoal + cellShiftDistance[static_cast<int8_t>(nextStep.direction)];
        }

        step.destination = {CellColToWorldX(mapCol), CellRowToWorldY(mapRow), 0.0f};
        step.tileR = static_cast<int16_t>(mapRow / 3);
        step.tileC = static_cast<int16_t>(mapCol / 3);
        step.cellR = static_cast<int16_t>(mapRow - step.tileR * 3);
        step.cellC = static_cast<int16_t>(mapCol - step.tileC * 3);
        node.flags |= CELL_ON_PATH;
        r += cellShift[node.parent * 2];
        c += cellShift[node.parent * 2 + 1];
    }

    return path->numSteps;
}

void MoveMap::destroy()
{
    if (map != nullptr)
    {
        std::free(map);
        map = nullptr;
    }

    if (openList != nullptr)
    {
        PriorityQueue* queue = openList;
        queue->destroy();
        delete queue;
        openList = nullptr;
    }
}
