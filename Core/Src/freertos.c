/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "debug.h"
#include "spi_flash.h"
#include "sfud.h"
#include "lfs.h"
#include "lfs_port.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */

  log_rtt_println("=== LittleFS Test ===");

  lfs_port_init();
  log_rtt_println("Port init OK");

  lfs_t lfs;
  int err = lfs_mount(&lfs, &g_lfs_cfg);
  if (err) {
    log_rtt_printf("Mount failed: %d, formatting...\r\n", err);
    err = lfs_format(&lfs, &g_lfs_cfg);
    if (err) {
      log_rtt_printf("Format FAILED: %d\r\n", err);
      for(;;) {}
    }
    log_rtt_println("Format OK, remounting...");
    err = lfs_mount(&lfs, &g_lfs_cfg);
    if (err) {
      log_rtt_printf("Remount FAILED: %d\r\n", err);
      for(;;) {}
    }
  }
  log_rtt_println("LFS mounted OK");

  const char *msg = "Hello from LittleFS on W25Q64!";
  lfs_file_t file;

  err = lfs_file_open(&lfs, &file, "test.txt", LFS_O_RDWR | LFS_O_CREAT);
  if (err) {
    log_rtt_printf("File open FAILED: %d\r\n", err);
    for(;;) {};
  }
  log_rtt_println("File opened: test.txt");

  lfs_ssize_t w = lfs_file_write(&lfs, &file, msg, strlen(msg));
  if (w < 0) {
    log_rtt_printf("File write FAILED: %d\r\n", (int)w);
    for(;;) {};
  }
  log_rtt_printf("Written: %d bytes\r\n", (int)w);

  err = lfs_file_close(&lfs, &file);
  if (err) {
    log_rtt_printf("File close FAILED: %d\r\n", err);
    for(;;) {};
  }
  log_rtt_println("File closed OK");

  err = lfs_file_open(&lfs, &file, "test.txt", LFS_O_RDONLY);
  if (err) {
    log_rtt_printf("File re-open FAILED: %d\r\n", err);
    for(;;) {};
  }

  char buf[64] = {0};
  lfs_ssize_t r = lfs_file_read(&lfs, &file, buf, sizeof(buf) - 1);
  if (r < 0) {
    log_rtt_printf("File read FAILED: %d\r\n", (int)r);
    for(;;) {};
  }
  log_rtt_printf("Read back: %d bytes → \"%s\"\r\n", (int)r, buf);

  err = lfs_file_close(&lfs, &file);

  if (strcmp(buf, msg) == 0) {
    log_rtt_println("=== LittleFS Test PASSED ===");
  } else {
    log_rtt_println("=== LittleFS Test FAILED (data mismatch) ===");
  }

  /* Infinite loop */
  for(;;)
  {
    HAL_GPIO_TogglePin(GREEN_GPIO_Port, GREEN_Pin);
    osDelay(500);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

