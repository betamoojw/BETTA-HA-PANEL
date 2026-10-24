/* SPDX-License-Identifier: LicenseRef-FNCL-1.1 | Copyright (c) 2026 andre68723 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui/ui_alert_center.h"
#include "ui/alert_policy.h"
#include "ui/ui_pages.h"
#include "ui/ui_i18n.h"
#include "ui/fonts/app_symbols.h"
static lv_obj_t *find(lv_obj_t *root, const char *value)
{
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), value)) return root;
    for (uint32_t i=0;i<lv_obj_get_child_count(root);i++) {
        lv_obj_t *found=find(lv_obj_get_child(root,i),value); if(found)return found;
    }
    return NULL;
}
static void widths(lv_obj_t *obj)
{
    if (lv_obj_check_type(obj,&lv_label_class))
        assert(lv_obj_get_width(obj)<=lv_obj_get_content_width(lv_obj_get_parent(obj)));
    for(uint32_t i=0;i<lv_obj_get_child_count(obj);i++)widths(lv_obj_get_child(obj,i));
}
int sim_alert_center_regression(void)
{
    bool primed=false, active=false; char last[80]="";
    assert(!alert_policy_sample(false,&primed,&active,last,sizeof(last),"on") && active);
    assert(!alert_policy_sample(false,&primed,&active,last,sizeof(last),"unavailable") && active);
    assert(!alert_policy_sample(false,&primed,&active,last,sizeof(last),"off") && !active);
    assert(alert_policy_sample(false,&primed,&active,last,sizeof(last),"on") && active);
    assert(!alert_policy_sample(false,&primed,&active,last,sizeof(last),"on"));
    primed=false;active=false;last[0]=0;
    assert(!alert_policy_sample(true,&primed,&active,last,sizeof(last),"unknown") && !primed);
    assert(!alert_policy_sample(true,&primed,&active,last,sizeof(last),"first"));
    assert(alert_policy_sample(true,&primed,&active,last,sizeof(last),"second"));
    assert(!alert_policy_sample(true,&primed,&active,last,sizeof(last),"unavailable"));
    assert(!alert_policy_sample(true,&primed,&active,last,sizeof(last),"second"));
    assert(alert_policy_connections(29999,true,false,true,false,true,false)==0);
    assert(alert_policy_connections(30000,true,false,true,false,true,false)==1);
    assert(alert_policy_connections(30000,true,true,true,false,true,false)==6);
    assert(alert_policy_connections(30000,true,true,false,false,false,false)==0);
    assert(alert_policy_connections(30000,true,true,true,true,true,true)==0);
    const char *langs[]={"en","de","es","fr","pl","zh-cn","zh-tw"};
    for(unsigned l=0;l<7;l++) {
        assert(ui_i18n_init(langs[l])==ESP_OK);ui_pages_init();
        for(unsigned i=0;i<UI_ALERT_SLOTS;i++)ui_alert_center_clear(i);
        ui_alert_center_refresh(0);
        lv_obj_t *icon=find(lv_screen_active(),APP_SYMBOL_ALERT);assert(icon);
        lv_obj_t *button=lv_obj_get_parent(icon);assert(lv_obj_is_hidden(button));
        coredump_record_t report={0};memset(report.id,'a',64);strcpy(report.task,"lvgl");
        strcpy(report.panic_reason,"A retained diagnostic with a long reason that must wrap inside the card on compact screens.");
        report.diagnostic.cause=COREDUMP_CAUSE_PPA_TIMEOUT;
        ui_alert_center_crash(0,&report);
        ui_alert_center_set(UI_ALERT_ENTITY_0,"Doorbell / 门铃 / 門鈴","Front door event. This entry expires without deleting crash reports.",1000);
        ui_alert_center_refresh(100);assert(ui_alert_center_count()==2 && !lv_obj_is_hidden(button));
        lv_obj_send_event(button,LV_EVENT_CLICKED,NULL);assert(ui_alert_center_root());
        lv_obj_update_layout(ui_alert_center_root());widths(ui_alert_center_root());
        lv_obj_t *root=ui_alert_center_root();ui_alert_center_open();assert(root==ui_alert_center_root());
        lv_obj_t *panel=lv_obj_get_child(root,0), *body=lv_obj_get_child(panel,2);
        lv_obj_scroll_to_y(body,10000,LV_ANIM_OFF);lv_obj_update_layout(root);
        lv_area_t body_area,last_area;
        lv_obj_get_coords(body,&body_area);lv_obj_get_coords(lv_obj_get_child(body,-1),&last_area);
        assert(last_area.y2<=body_area.y2); /* final alert remains reachable */
        ui_alert_center_refresh(1000);assert(ui_alert_center_count()==1);
        ui_alert_center_clear(0);ui_alert_center_refresh(1001);
        assert(ui_alert_center_count()==0 && lv_obj_is_hidden(button));
        assert(find(root,ui_i18n_get("alerts.empty","No alerts")));
        lv_obj_t *close=find(root,APP_SYMBOL_CLOSE);assert(close);
        lv_area_t r;lv_obj_get_coords(lv_obj_get_parent(close),&r);
        assert(r.x1>=0 && r.y1>=0 && r.x2<lv_display_get_horizontal_resolution(lv_display_get_default()));
        lv_obj_send_event(lv_obj_get_parent(close),LV_EVENT_CLICKED,NULL);assert(!ui_alert_center_root());
        ui_alert_center_open();ui_pages_init();assert(!ui_alert_center_root());
    }
    puts("PASS: alert policies, seven languages, expiry/recovery, live empty state, close and rebuild");return 0;
}
