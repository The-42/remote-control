/*
 * Copyright (C) 2012 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef REMOTE_CONTROL_LOG_H
#define REMOTE_CONTROL_LOG_H 1

#include <glib.h>

int remote_control_log_init(GKeyFile *conf);
void remote_control_log_exit(void);

#endif /* REMOTE_CONTROL_LOG_H */
