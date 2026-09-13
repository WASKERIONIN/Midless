/**
 * Copyright (c) 2021-2022 Sirvoid
 * 
 * This software is released under the MIT License.
 * https://opensource.org/licenses/MIT
 */

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

void ServerLogger_Log(const char* message) {
    /* v65.7: "%s\n", not printf(message) - server names come from
     * server.ini and must never be format strings; logs get line ends */
    printf("%s\n", message);
    fflush(stdout);
}