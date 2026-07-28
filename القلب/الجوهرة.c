#include "الجوهرة.h"
#include "محرك_الذاكرة/بوابة_الذاكرة.h"
#include "محرك_الجدولة/بوابة_الجدولة.h"
#include "محرك_العمليات/بوابة_العمليات.h"

void core_init() {

    memory_api.init();
    
    process_api.init();   

    scheduler_api.init();
   
}
void core_run() {

    scheduler_api.schedule();

}

