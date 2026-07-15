#include <string.h>

#include "board.h"
#include "bflb_pec_v2_instance.h"
#include "bf3901.h"

struct bflb_pec_dvp_cam_cfg_match_s {
    char *name;
    struct bflb_pec_dvp_cam_s *cfg;
};

static struct bflb_pec_dvp_cam_cfg_match_s cam_cfg_match_arr[] = {
    { "BF3901", &cam_cfg_bf3901, },
};

struct bflb_pec_dvp_cam_s *bflb_pec_dvp_cam_get_cfg(char *name)
{
    for (int i = 0; i < sizeof(cam_cfg_match_arr) / sizeof(cam_cfg_match_arr[0]); i++) {
        if (strcmp(name, cam_cfg_match_arr[i].name) == 0) {
            return cam_cfg_match_arr[i].cfg;
        }
    }
    return NULL;
}
