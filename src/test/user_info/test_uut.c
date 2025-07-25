#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "user_info.h"
#include <check.h>

user_info_holder queue_users;

unsigned int get_num_queued(user_info_holder *, const char *);
unsigned int count_jobs_submitted(job *);


START_TEST(remove_server_suffix_test)
  {
  std::string u1("dbeer@napali");
  std::string u2("dbeer");
  std::string u3("dbeer@waimea");

  remove_server_suffix(u1);
  remove_server_suffix(u2);
  remove_server_suffix(u3);

  fail_unless(!strcmp(u1.c_str(), "dbeer"));
  fail_unless(!strcmp(u2.c_str(), "dbeer"));
  fail_unless(!strcmp(u3.c_str(), "dbeer"));
  }
END_TEST



START_TEST(get_num_queued_test)
  {
  unsigned int queued;

  // users
  users.lock();
  users.clear();
  users.unlock();
  user_info ui;

  ui.user_name = (char *)"tom";
  ui.num_jobs_queued = 1;

  users.lock();
  users.insert(&ui,ui.user_name);
  users.unlock();

  queued = get_num_queued(&users, "bob");
  fail_unless(queued == 0, "incorrect queued count for bob");

  queued = get_num_queued(&users, "tom");
  fail_unless(queued == 1, "incorrect queued count for tom");

  // queue_users
  queue_users.lock();
  queue_users.clear();
  queue_users.unlock();

  ui.user_name = (char *)"jerry";
  ui.num_jobs_queued = 1;

  queue_users.lock();
  queue_users.insert(&ui,ui.user_name);
  queue_users.unlock();

  queued = get_num_queued(&queue_users, "jerry");
  fail_unless(queued == 1, "incorrect queued count for jerry");
  }
END_TEST




START_TEST(count_jobs_submitted_test)
  {
  unsigned int submitted;
  job          pjob;

  memset(&pjob, 0, sizeof(pjob));
  users.lock();
  users.clear();
  users.unlock();

  submitted = count_jobs_submitted(&pjob);
  fail_unless(submitted == 1, "incorrect count for non-array job");

  pjob.ji_wattr[JOB_ATR_job_array_request].at_val.at_str = (char *)"0-10";
  submitted = count_jobs_submitted(&pjob);
  fail_unless(submitted == 11, "incorrect count for 0-10");
  }
END_TEST



START_TEST(can_queue_new_job_test)
  {
  job pjob;

  memset(&pjob, 0, sizeof(pjob));

  // users
  users.lock();
  users.clear();

  user_info *ui = (user_info *)calloc(1,sizeof(user_info));

  ui->user_name = strdup("tom");
  ui->num_jobs_queued = 1;

  users.insert(ui,ui->user_name);

  ui = (user_info *)calloc(1,sizeof(user_info));

  ui->user_name = strdup("bob");
  ui->num_jobs_queued = 0;

  users.insert(ui,ui->user_name);
  users.unlock();


  fail_unless(can_queue_new_job((char *)"bob", &pjob) == TRUE, "user without a job can't queue one?");
  fail_unless(can_queue_new_job((char *)"tom", &pjob) == FALSE, (char *)"tom allowed over limit");
  pjob.ji_wattr[JOB_ATR_job_array_request].at_val.at_str = (char *)"0-10";

  fail_unless(can_queue_new_job((char *)"bob", &pjob) == FALSE, "array job allowed over limit");
  fail_unless(can_queue_new_job((char *)"tom", &pjob) == FALSE, "array job allowed over limit");

  // queue_users
  queue_users.lock();
  queue_users.clear();

  ui = (user_info *)calloc(1,sizeof(user_info));

  ui->user_name = strdup("jerry");
  ui->num_jobs_queued = 1;

  queue_users.insert(ui,ui->user_name);
  queue_users.unlock();


  fail_unless(can_queue_new_job((char *)"jerry", &pjob) == FALSE, (char *)"jerry allowed over limit");
  pjob.ji_wattr[JOB_ATR_job_array_request].at_val.at_str = (char *)"0-10";

  fail_unless(can_queue_new_job((char *)"jerry", &pjob) == FALSE, "array job allowed over limit");
  }
END_TEST



START_TEST(increment_queued_jobs_test)
  {
  job pjob;

  memset(&pjob, 0, sizeof(pjob));

  // users
  users.lock();
  users.clear();

  user_info *ui = (user_info *)calloc(1,sizeof(user_info));

  ui->user_name = strdup("tom");
  ui->num_jobs_queued = 1;

  users.insert(ui,ui->user_name);

  ui = (user_info *)calloc(1,sizeof(user_info));

  ui->user_name = strdup("bob");
  ui->num_jobs_queued = 0;

  users.insert(ui,ui->user_name);
  users.unlock();

  // queue_users
  queue_users.lock();
  queue_users.clear();

  ui = (user_info *)calloc(1,sizeof(user_info));

  ui->user_name = strdup("jerry");
  ui->num_jobs_queued = 1;

  queue_users.insert(ui,ui->user_name);
  queue_users.unlock();

  // users and queue_users
  // ensure bit is reset correctly after incrementing
  fail_unless(increment_queued_jobs(&users, (char *)"tom", &pjob) == 0, "can't increment queued jobs");
  fail_unless(increment_queued_jobs(&queue_users, (char *)"jerry", &pjob) == 0, "can't increment queued jobs");
  fail_unless(pjob.ji_queue_counted == (COUNTED_GLOBALLY | COUNTED_IN_QUEUE));
  //  after 1 increment the count should be 2 because initialize_user_info() starts out with tom and jerry at 1
  fail_unless(get_num_queued(&users, "tom") == 2, "didn't actually increment tom 1");
  fail_unless(get_num_queued(&queue_users, "jerry") == 2, "didn't actually increment tom 1");
  pjob.ji_queue_counted = 0;

  // users
  fail_unless(increment_queued_jobs(&users, (char *)"bob", &pjob) == 0, "can't increment queued jobs");
  fail_unless(get_num_queued(&users, "bob") == 1, "didn't actually increment bob 0");
  pjob.ji_queue_counted = 0;
  // after 2 increments the count should be 3 because initialize_user_info() starts out with tom at 
  // 1 instead of 0, as a normal program would start. Its done this way for the decrement code.
  fail_unless(increment_queued_jobs(&users, strdup("tom@napali"), &pjob) == 0);
  fail_unless(get_num_queued(&users, "tom") == 3, "didn't actually increment tom 2");

  // queue_users
  pjob.ji_queue_counted = 0;
  fail_unless(increment_queued_jobs(&queue_users, (char *)"jerry", &pjob) == 0, "can't increment queued jobs");
  fail_unless(get_num_queued(&queue_users, "jerry") == 3, "didn't actually increment jerry 2");

  // users
  // Enqueue the job without resetting to make sure the count doesn't change
  pjob.ji_queue_counted = COUNTED_GLOBALLY;
  fail_unless(increment_queued_jobs(&users, strdup("tom@napali"), &pjob) == 0);
  fail_unless(get_num_queued(&users, "tom") == 3, "Shouldn't have incremented with the bit set");

  // queue_users
  // Enqueue the job without resetting to make sure the count doesn't change
  pjob.ji_queue_counted = COUNTED_IN_QUEUE;
  fail_unless(increment_queued_jobs(&queue_users, strdup("jerry@napali"), &pjob) == 0);
  fail_unless(get_num_queued(&queue_users, "jerry") == 3, "Shouldn't have incremented with the bit set");

  // users
  // Make sure array subjobs are a no-op
  pjob.ji_queue_counted = 0;
  pjob.ji_is_array_template = FALSE;
  pjob.ji_wattr[JOB_ATR_job_array_id].at_flags = ATR_VFLAG_SET;
  fail_unless(increment_queued_jobs(&users, strdup("tom"), &pjob) == 0);
  fail_unless(get_num_queued(&users, "tom") == 3, "Shouldn't have incremented for an array sub-job");
  fail_unless(pjob.ji_queue_counted != 0);
  
  pjob.ji_queue_counted = 0;
  pjob.ji_wattr[JOB_ATR_job_array_id].at_flags = 0;
  fail_unless(increment_queued_jobs(&users, strdup("tom"), &pjob) == 0);
  fail_unless(get_num_queued(&users, "tom") == 4, "Should have incremented for non array sub-job");

  // test array job

  // set array string
  pjob.ji_wattr[JOB_ATR_job_array_request].at_val.at_str = strdup("0-10");

  // users
  pjob.ji_queue_counted = 0;
  pjob.ji_is_array_template = TRUE;
  fail_unless(increment_queued_jobs(&users, strdup("tom"), &pjob) == 0);
  fail_unless(get_num_queued(&users, "tom") == 15, "didn't actually increment tom 15");

  // queue_users
  pjob.ji_queue_counted = 0;
  pjob.ji_is_array_template = TRUE;
  fail_unless(increment_queued_jobs(&queue_users, strdup("jerry"), &pjob) == 0);
  fail_unless(get_num_queued(&queue_users, "jerry") == 14, "didn't actually increment jerry 14");
  }
END_TEST



START_TEST(decrement_queued_jobs_test)
  {
  // users
  users.lock();
  users.clear();
  user_info *ui = (user_info *)calloc(1,sizeof(user_info));

  ui->user_name = strdup("tom"); 
  ui->num_jobs_queued = 1;
  job pjob;
  memset(&pjob, 0, sizeof(pjob));

  users.insert(ui,ui->user_name);
  users.unlock();

  // queue users
  queue_users.lock();
  queue_users.clear();
  ui = (user_info *)calloc(1,sizeof(user_info));

  ui->user_name = strdup("jerry");
  ui->num_jobs_queued = 1;
  memset(&pjob, 0, sizeof(pjob));

  queue_users.insert(ui,ui->user_name);
  queue_users.unlock();


  // users
  pjob.ji_queue_counted = COUNTED_GLOBALLY;
  fail_unless(decrement_queued_jobs(&users, (char *)"bob", &pjob) == THING_NOT_FOUND, "decremented for non-existent user");
  pjob.ji_queue_counted = COUNTED_GLOBALLY;
  fail_unless(decrement_queued_jobs(&users, (char *)"tom@neverland.com", &pjob) == 0, "couldn't decrement for tom?");

  fail_unless(get_num_queued(&users, "tom") == 0, "didn't actually decrement tom");
  pjob.ji_queue_counted = COUNTED_GLOBALLY;
  
  fail_unless(decrement_queued_jobs(&users, (char *)"tom", &pjob) == 0,
    "couldn't decrement for tom?");
  fail_unless(get_num_queued(&users, "tom") == 0, "Disallow going negative in the count");

  // Make sure we are clearing and resetting the bits  
  fail_unless(increment_queued_jobs(&users, strdup("tom@napali"), &pjob) == 0);
  fail_unless(get_num_queued(&users, "tom") == 1);
  fail_unless(decrement_queued_jobs(&users, (char *)"tom", &pjob) == 0);
  fail_unless(get_num_queued(&users, "tom") == 0);

  // queue_users
  pjob.ji_queue_counted = COUNTED_IN_QUEUE;
  fail_unless(decrement_queued_jobs(&queue_users, (char *)"jerry@neverland.com", &pjob) == 0, "couldn't decrement for jerry?");
  fail_unless(get_num_queued(&queue_users, "jerry") == 0, "didn't actually decrement jerry");

  fail_unless(decrement_queued_jobs(&queue_users, (char *)"jerry@neverland.com", &pjob) == 0, "couldn't decrement for jerry?");
  fail_unless(get_num_queued(&queue_users, "jerry") == 0, "preventing redecrementng the same job twice");

  pjob.ji_queue_counted = COUNTED_IN_QUEUE;
  fail_unless(decrement_queued_jobs(&queue_users, (char *)"jerry", &pjob) == 0, "couldn't decrement for jerry?");
  fail_unless(get_num_queued(&queue_users, "jerry") == 0, "Disallow going negative in the count");

  // Make sure we are clearing and resetting the bits  
  fail_unless(increment_queued_jobs(&queue_users, strdup("jerry@napali"), &pjob) == 0);
  fail_unless(get_num_queued(&queue_users, "jerry") == 1);
  fail_unless(decrement_queued_jobs(&queue_users, (char *)"jerry", &pjob) == 0);
  fail_unless(get_num_queued(&queue_users, "jerry") == 0);

  // test array job

  // set array string
  pjob.ji_wattr[JOB_ATR_job_array_request].at_val.at_str = strdup("0-10");

  // users
  // first increment
  pjob.ji_queue_counted = 0;
  pjob.ji_is_array_template = TRUE;
  fail_unless(increment_queued_jobs(&users, strdup("tom"), &pjob) == 0);
  fail_unless(get_num_queued(&users, "tom") == 11);

  // now decrement
  pjob.ji_queue_counted = COUNTED_GLOBALLY;
  pjob.ji_is_array_template = TRUE;
  fail_unless(decrement_queued_jobs(&users, (char *)"tom", &pjob) == 0);
  fail_unless(get_num_queued(&users, "tom") == 0);

  // queue_users
  // first increment
  pjob.ji_queue_counted = 0;
  pjob.ji_is_array_template = TRUE;
  fail_unless(increment_queued_jobs(&queue_users, strdup("jerry"), &pjob) == 0);
  fail_unless(get_num_queued(&queue_users, "jerry") == 11);

  // now decrement
  pjob.ji_queue_counted = COUNTED_IN_QUEUE;
  pjob.ji_is_array_template = TRUE;
  fail_unless(decrement_queued_jobs(&queue_users, (char *)"jerry", &pjob) == 0);
  fail_unless(get_num_queued(&queue_users, "jerry") == 0);

  }
END_TEST




Suite *user_info_suite(void)
  {
  Suite *s = suite_create("user_info test suite methods");
  TCase *tc_core = tcase_create("get_num_queued_test");
  tcase_add_test(tc_core, get_num_queued_test);
  suite_add_tcase(s, tc_core);
  
  tc_core = tcase_create("count_jobs_submitted_test");
  tcase_add_test(tc_core, count_jobs_submitted_test);
  suite_add_tcase(s, tc_core);
  
  tc_core = tcase_create("can_queue_new_job_test");
  tcase_add_test(tc_core, can_queue_new_job_test);
  suite_add_tcase(s, tc_core);

  tc_core = tcase_create("increment_queued_jobs_test");
  tcase_add_test(tc_core, increment_queued_jobs_test);
  suite_add_tcase(s, tc_core);

  tc_core = tcase_create("decrement_queued_jobs_test");
  tcase_add_test(tc_core, decrement_queued_jobs_test);
  tcase_add_test(tc_core, remove_server_suffix_test);
  suite_add_tcase(s, tc_core);
  
  return(s);
  }



void rundebug()
  {
  }



int main(void)
  {
  int number_failed = 0;
  SRunner *sr = NULL;
  rundebug();
  sr = srunner_create(user_info_suite());
  srunner_set_log(sr, "user_info_suite.log");
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return(number_failed);
  }

