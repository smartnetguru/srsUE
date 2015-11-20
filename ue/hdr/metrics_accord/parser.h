#ifndef __PARSER_H__
#define __PARSER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ue_status.h"

int ue_status_to_xml(ue_status_t *status, char *xml, int n);

#ifdef __cplusplus
}
#endif

#endif /* __PARSER_H__ */
