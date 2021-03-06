#include "UnrealEnginePythonPrivatePCH.h"

#include "PythonDelegate.h"
#include "PythonFunction.h"

#if WITH_EDITOR
#include "Runtime/AssetRegistry/Public/AssetRegistryModule.h"
#endif

PyObject *py_ue_get_class(ue_PyUObject * self, PyObject * args) {

	ue_py_check(self);

	ue_PyUObject *ret = ue_get_python_wrapper(self->ue_object->GetClass());
	if (!ret)
		return PyErr_Format(PyExc_Exception, "PyUObject is in invalid state");
	Py_INCREF(ret);
	return (PyObject *)ret;
}

PyObject *py_ue_is_a(ue_PyUObject * self, PyObject * args) {

	ue_py_check(self);

	PyObject *obj;
	if (!PyArg_ParseTuple(args, "O:is_a", &obj)) {
		return NULL;
	}

	if (!ue_is_pyuobject(obj)) {
		return PyErr_Format(PyExc_Exception, "argument is not a UObject");
	}

	ue_PyUObject *py_obj = (ue_PyUObject *)obj;

	if (self->ue_object->IsA((UClass *)py_obj->ue_object)) {
		Py_INCREF(Py_True);
		return Py_True;
	}


	Py_INCREF(Py_False);
	return Py_False;
}

PyObject *py_ue_call_function(ue_PyUObject * self, PyObject * args) {

	ue_py_check(self);

	UFunction *function = nullptr;

	if (PyTuple_Size(args) < 1) {
		return PyErr_Format(PyExc_TypeError, "this function requires at least an argument");
	}

	PyObject *func_id = PyTuple_GetItem(args, 0);

	if (PyUnicode_Check(func_id)) {
		function = self->ue_object->FindFunction(FName(UTF8_TO_TCHAR(PyUnicode_AsUTF8(func_id))));
	}


	if (!function)
		return PyErr_Format(PyExc_Exception, "unable to find function");

	uint8 *buffer = (uint8 *)FMemory_Alloca(function->ParmsSize);
	int argn = 1;

	TFieldIterator<UProperty> PArgs(function);
	for (; PArgs && ((PArgs->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm); ++PArgs) {
		UProperty *prop = *PArgs;
		PyObject *py_arg = PyTuple_GetItem(args, argn);
		if (!py_arg) {
			return PyErr_Format(PyExc_TypeError, "not enough arguments");
		}
		if (!ue_py_convert_pyobject(py_arg, prop, buffer)) {
			return PyErr_Format(PyExc_TypeError, "unable to convert pyobject to property");
		}
		argn++;
	}

	self->ue_object->ProcessEvent(function, buffer);

	TFieldIterator<UProperty> Props(function);
	for (; Props; ++Props) {
		UProperty *prop = *Props;
		if (prop->GetPropertyFlags() & CPF_ReturnParm) {
			return ue_py_convert_property(prop, buffer);
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *py_ue_find_function(ue_PyUObject * self, PyObject * args) {

	ue_py_check(self);

	char *name;
	if (!PyArg_ParseTuple(args, "s:find_function", &name)) {
		return NULL;
	}

	UFunction *function = self->ue_object->FindFunction(FName(UTF8_TO_TCHAR(name)));
	if (!function) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	UE_LOG(LogPython, Warning, TEXT("Func %d %d"), function->NumParms, function->ReturnValueOffset);

	ue_PyUObject *ret = ue_get_python_wrapper((UObject *)function);
	if (!ret)
		return PyErr_Format(PyExc_Exception, "PyUObject is in invalid state");
	Py_INCREF(ret);
	return (PyObject *)ret;

}

PyObject *py_ue_get_name(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);


	return PyUnicode_FromString(TCHAR_TO_UTF8(*(self->ue_object->GetName())));
}

PyObject *py_ue_get_full_name(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);


	return PyUnicode_FromString(TCHAR_TO_UTF8(*(self->ue_object->GetFullName())));
}

PyObject *py_ue_set_property(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);

	char *property_name;
	PyObject *property_value;
	if (!PyArg_ParseTuple(args, "sO:set_property", &property_name, &property_value)) {
		return NULL;
	}

	UStruct *u_struct = nullptr;

	if (self->ue_object->IsA<UStruct>()) {
		u_struct = (UStruct *)self->ue_object;
	}
	else {
		u_struct = (UStruct *)self->ue_object->GetClass();
	}

	UProperty *u_property = u_struct->FindPropertyByName(FName(UTF8_TO_TCHAR(property_name)));
	if (!u_property)
		return PyErr_Format(PyExc_Exception, "unable to find property %s", property_name);


	if (!ue_py_convert_pyobject(property_value, u_property, (uint8 *)self->ue_object)) {
		return PyErr_Format(PyExc_Exception, "unable to set property %s", property_name);
	}

	Py_INCREF(Py_None);
	return Py_None;

}

PyObject *py_ue_properties(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);

	UStruct *u_struct = nullptr;

	if (self->ue_object->IsA<UStruct>()) {
		u_struct = (UStruct *)self->ue_object;
	}
	else {
		u_struct = (UStruct *)self->ue_object->GetClass();
	}

	PyObject *ret = PyList_New(0);

	for (TFieldIterator<UProperty> PropIt(u_struct); PropIt; ++PropIt)
	{
		UProperty* property = *PropIt;
		PyObject *property_name = PyUnicode_FromString(TCHAR_TO_UTF8(*property->GetName()));
		PyList_Append(ret, property_name);
		Py_DECREF(property_name);
	}

	return ret;

}

PyObject *py_ue_call(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);

	char *call_args;
	if (!PyArg_ParseTuple(args, "s:call", &call_args)) {
		return NULL;
	}

	FOutputDeviceNull od_null;
	if (!self->ue_object->CallFunctionByNameWithArguments(UTF8_TO_TCHAR(call_args), od_null, NULL, true)) {
		return PyErr_Format(PyExc_Exception, "error while calling \"%s\"", call_args);
	}

	Py_INCREF(Py_None);
	return Py_None;

}

PyObject *py_ue_get_property(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);

	char *property_name;
	if (!PyArg_ParseTuple(args, "s:get_property", &property_name)) {
		return NULL;
	}

	UStruct *u_struct = nullptr;

	if (self->ue_object->IsA<UStruct>()) {
		u_struct = (UStruct *)self->ue_object;
	}
	else {
		u_struct = (UStruct *)self->ue_object->GetClass();
	}

	UProperty *u_property = u_struct->FindPropertyByName(FName(UTF8_TO_TCHAR(property_name)));
	if (!u_property)
		return PyErr_Format(PyExc_Exception, "unable to find property %s", property_name);

	return ue_py_convert_property(u_property, (uint8 *)self->ue_object);
}

PyObject *py_ue_is_rooted(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);

	if (self->ue_object->IsRooted()) {
		Py_INCREF(Py_True);
		return Py_True;
	}

	Py_INCREF(Py_False);
	return Py_False;
}


PyObject *py_ue_add_to_root(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);

	self->ue_object->AddToRoot();

	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *py_ue_remove_from_root(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);

	self->ue_object->RemoveFromRoot();

	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *py_ue_bind_event(ue_PyUObject * self, PyObject * args) {

	ue_py_check(self);

	char *event_name;
	PyObject *py_callable;
	if (!PyArg_ParseTuple(args, "sO:bind_event", &event_name, &py_callable)) {
		return NULL;
	}
	
	if (!PyCallable_Check(py_callable)) {
		return PyErr_Format(PyExc_Exception, "object is not callable");
	}

	return ue_bind_pyevent(self, FString(event_name), py_callable, true);
}

PyObject *py_ue_add_function(ue_PyUObject * self, PyObject * args) {

	ue_py_check(self);

	char *name;
	PyObject *py_callable;
	if (!PyArg_ParseTuple(args, "sO:add_function", &name, &py_callable)) {
		return NULL;
	}

	if (!self->ue_object->IsA<UClass>()) {
		return PyErr_Format(PyExc_Exception, "uobject is not a UClass");
	}

	UClass *u_class = (UClass *)self->ue_object;

	if (!PyCallable_Check(py_callable)) {
		return PyErr_Format(PyExc_Exception, "object is not callable");
	}

	if (u_class->FindFunctionByName(UTF8_TO_TCHAR(name))) {
		return PyErr_Format(PyExc_Exception, "function %s is already registered", name);
	}

	UFunction *parent_function = u_class->GetSuperClass()->FindFunctionByName(UTF8_TO_TCHAR(name));

	UPythonFunction *function = NewObject<UPythonFunction>(u_class, UTF8_TO_TCHAR(name), RF_Public);
	function->SetPyCallable(py_callable);
	function->RepOffset = MAX_uint16;
	function->ReturnValueOffset = MAX_uint16;
	function->FirstPropertyToInit = NULL;

	function->Script.Add(EX_EndFunctionParms);

	if (parent_function) {
		function->SetSuperStruct(parent_function);
		/*
			duplicate properties
		*/
	}
	else {
		UField **next_property = &function->Children;
		UProperty **next_property_link = &function->PropertyLink;
		// get __annotations__
		PyObject *annotations = PyObject_GetAttrString(py_callable, "__annotations__");
		PyObject *annotation_keys = PyDict_Keys(annotations);
		if (annotation_keys) {
			Py_ssize_t len = PyList_Size(annotation_keys);
			for (Py_ssize_t i = 0; i < len; i++) {
				PyObject *key = PyList_GetItem(annotation_keys, i);
				char *p_name = PyUnicode_AsUTF8(key);
				PyObject *value = PyDict_GetItem(annotations, key);
				UProperty *prop = nullptr;
				if (PyType_Check(value)) {
					if ((PyTypeObject *)value == &PyFloat_Type) {
						prop = NewObject<UFloatProperty>(function, UTF8_TO_TCHAR(p_name));
					}
				}
				if (prop) {
					if (!strcmp(p_name, "return")) {
						prop->SetPropertyFlags(CPF_Parm|CPF_ReturnParm);
					}
					else {
						prop->SetPropertyFlags(CPF_Parm);
					}
					*next_property = prop;
					next_property = &prop->Next;

					*next_property_link = prop;
					next_property_link = &prop->PropertyLinkNext;
				}
			}
		}
	}
	
	function->FunctionFlags |= FUNC_Native | FUNC_BlueprintCallable;
	FNativeFunctionRegistrar::RegisterFunction(u_class, UTF8_TO_TCHAR(name), (Native)&UPythonFunction::CallPythonCallable);

	function->Bind();
	function->StaticLink(true);

	// allocate properties storage (ignore super)
	TFieldIterator<UProperty> props(function, EFieldIteratorFlags::ExcludeSuper);
	for (; props; ++props) {
		UProperty *p = *props;
		if (p->HasAnyPropertyFlags(CPF_Parm)) {
			function->NumParms++;
			function->ParmsSize = p->GetOffset_ForUFunction() + p->GetSize();
			if (p->HasAnyPropertyFlags(CPF_ReturnParm)) {
				function->ReturnValueOffset = p->GetOffset_ForUFunction();
				p->DestructorLinkNext = function->DestructorLink;
				function->DestructorLink = p;
			}
		}
	}

	function->SetNativeFunc((Native)&UPythonFunction::CallPythonCallable);

	function->Next = u_class->Children;
	u_class->Children = function;
	u_class->AddFunctionToFunctionMap(function);

	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *py_ue_add_property(ue_PyUObject * self, PyObject * args) {

	ue_py_check(self);

	PyObject *obj;
	char *name;
	PyObject *replicate = nullptr;
	if (!PyArg_ParseTuple(args, "Os|O:add_property", &obj, &name, &replicate)) {
		return NULL;
	}

	if (!self->ue_object->IsA<UStruct>())
		return PyErr_Format(PyExc_Exception, "uobject is not a UStruct");

	if (!ue_is_pyuobject(obj)) {
		return PyErr_Format(PyExc_Exception, "argument is not a UObject");
	}

	ue_PyUObject *py_obj = (ue_PyUObject *)obj;

	if (!py_obj->ue_object->IsA<UClass>()) {
		return PyErr_Format(PyExc_Exception, "uobject is not a UClass");
	}

	UProperty *u_property = NewObject<UProperty>(self->ue_object, (UClass *)py_obj->ue_object, UTF8_TO_TCHAR(name));
	if (!u_property)
		return PyErr_Format(PyExc_Exception, "unable to allocate new UProperty");

	uint64 flags = CPF_Edit;
	if (replicate && PyObject_IsTrue(replicate)) {
		flags |= CPF_Net;
	}

	u_property->SetPropertyFlags(flags);

	UStruct *u_struct = (UStruct *)self->ue_object;

	u_struct->AddCppProperty(u_property);

	ue_PyUObject *ret = ue_get_python_wrapper(u_property);
	if (!ret)
		return PyErr_Format(PyExc_Exception, "PyUObject is in invalid state");
	Py_INCREF(ret);
	return (PyObject *)ret;
	
}

PyObject *py_ue_as_dict(ue_PyUObject * self, PyObject * args) {

	ue_py_check(self);

	UStruct *u_struct = nullptr;

	if (self->ue_object->IsA<UStruct>()) {
		u_struct = (UStruct *)self->ue_object;
	}
	else {
		u_struct = (UStruct *)self->ue_object->GetClass();
	}
	
	PyObject *py_struct_dict = PyDict_New();
	TFieldIterator<UProperty> SArgs(u_struct);
	for (; SArgs; ++SArgs) {
		PyObject *struct_value = ue_py_convert_property(*SArgs, (uint8 *)self->ue_object);
		if (!struct_value) {
			Py_DECREF(py_struct_dict);
			return NULL;
		}
		PyDict_SetItemString(py_struct_dict, TCHAR_TO_UTF8(*SArgs->GetName()), struct_value);
	}
	return py_struct_dict;
}

#if WITH_EDITOR
PyObject *py_ue_save_package(ue_PyUObject * self, PyObject * args) {

	ue_py_check(self);

	char *name;
	if (!PyArg_ParseTuple(args, "s:save_package", &name)) {
		return NULL;
	}

	UPackage *package = CreatePackage(nullptr, UTF8_TO_TCHAR(name));
	if (!package)
		return PyErr_Format(PyExc_Exception, "unable to create package");

	FString filename = FPackageName::LongPackageNameToFilename(UTF8_TO_TCHAR(name), FPackageName::GetAssetPackageExtension());

	if (UPackage::SavePackage(package, self->ue_object, EObjectFlags::RF_NoFlags, *filename)) {
		Py_INCREF(Py_True);
		return Py_True;
	}

	Py_INCREF(Py_False);
	return Py_False;
}
#endif

