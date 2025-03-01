// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0+ OR GPL-3.0 WITH Qt-GPL-exception-1.0

#pragma once

constexpr auto iar_stm32h750b_discovery_baremetal_json = R"(
{
    "qulVersion": "2.3.0",
    "compatVersion": "1",
    "platform": {
        "id": "STM32H750B-DISCOVERY-BAREMETAL",
        "vendor": "ST",
        "colorDepths": [
            32
        ],
        "cmakeEntries": [
            {
                "id": "STM32CubeProgrammer_PATH",
                "label": "STM32CubeProgrammer",
                "type": "path",
                "setting": "Stm32CubeProgrammer",
                "defaultValue": {
                    "windows": "%{Env:PROGRAMFILES}/STMicroelectronics/STM32Cube/STM32CubeProgrammer/",
                    "linux": "%{Env:HOME}/STMicroelectronics/STM32Cube/STM32CubeProgrammer/"
                },
                "detectionPath": {
                    "windows": "bin/STM32_Programmer_CLI.exe",
                    "linux": "bin/STM32_Programmer.sh"
                },
                "optional": false,
                "addToSystemPath": true
            }
        ]
    },
    "toolchain": {
        "id": "iar",
        "versions": [
            "9.20.4"
        ],
        "compiler": {
            "id": "IARToolchain",
            "setting": "IARToolchain",
            "envVar": "IAR_ARM_COMPILER_DIR",
            "label": "IAR ARM Compiler",
            "cmakeVar": "QUL_TARGET_TOOLCHAIN_DIR",
            "type": "path",
            "versionDetection": {
                "filePattern": {
                    "windows": "bin/iccarm.exe",
                    "linux": "bin/iccarm"
                },
                "executableArgs": "--version",
                "regex": "\\bV(\\d+\\.\\d+\\.\\d+)\\.\\d+\\b"
            },
            "detectionPath": {
                "windows": "bin/iccarm.exe",
                "linux": "bin/iccarm"
            },
            "optional": false
        },
        "file": {
            "id": "IAR_CMAKE_TOOLCHAIN_FILE",
            "cmakeVar": "CMAKE_TOOLCHAIN_FILE",
            "type": "file",
            "defaultValue": "%{Qul_ROOT}/lib/cmake/Qul/toolchain/iar.cmake",
            "visible": false,
            "optional": false
        }
    },
    "boardSdk": {
        "envVar": "STM32Cube_FW_H7_SDK_PATH",
        "setting": "STM32Cube_FW_H7_SDK_PATH",
        "versions": [
            "1.10.0"
        ],
        "versionDetection": {
            "filePattern": "package.xml",
            "xmlElement": "PackDescription",
            "xmlAttribute": "Release",
            "regex": "\\b(\\d+\\.\\d+\\.\\d+)\\b"
        },
        "id": "ST_SDK_DIR",
        "label": "Board SDK for STM32H750B-Discovery",
        "cmakeVar": "QUL_BOARD_SDK_DIR",
        "type": "path",
        "optional": false
    }
}
)";
