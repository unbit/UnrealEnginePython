#pragma once

#include "UnrealEnginePython.h"


#include "UEPySDropTarget.h"

#include "Editor/EditorWidgets/Public/SAssetDropTarget.h"

extern PyTypeObject ue_PySVectorInputBoxType;

typedef struct {
	ue_PySDropTarget s_drop_target;
	/* Type-specific fields go here. */
} ue_PySAssetDropTarget;

void ue_python_init_sasset_drop_target(PyObject *);
