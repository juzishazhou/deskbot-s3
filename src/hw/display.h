#pragma once

#include <stdint.h>

#include "logic/behavior.h"

void displayInit();
bool displayReady();
void displaySetMood(Mood mood);
void displayTick(uint32_t now);
