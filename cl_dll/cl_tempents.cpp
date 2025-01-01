// BlueNightHawk - 2025
// Reference : Xash3D FWGS

#include "cl_dll.h"
#include "cl_util.h"
#include "r_efx.h"

#include "triangleapi.h"

#include "cl_tempents.h"

#include "com_model.h"
#include "studio.h"
#include "r_studioint.h"

extern engine_studio_api_s IEngineStudio;

void CTempEntManager::Init()
{
	cl_tempents = CVAR_CREATE("cl_tempents", "1", FCVAR_ARCHIVE);

	InitTempEnts();
}

void CTempEntManager::SetupTempEnts(TEMPENTITY** ppTempEntFree, TEMPENTITY** ppTempEntActive)
{
	if (m_bInitDone)
	{
		gEngfuncs.CL_CreateVisibleEntity(0, &m_DummyEnt);
		gEngfuncs.CL_CreateVisibleEntity(0, &m_DummyTransEnt);
		return;
	}

	m_DummyEnt.model = IEngineStudio.Mod_ForName("models/shell.mdl", 0);
	m_DummyTransEnt = m_DummyEnt;
	m_DummyTransEnt.curstate.rendermode = kRenderTransAdd;
	m_DummyTransEnt.curstate.renderamt = 1;
	m_DummyTransEnt.curstate.rendercolor = {255, 255, 255};

	*ppTempEntFree = m_TempEnts;
	*ppTempEntActive = nullptr;

	gEngfuncs.CL_CreateVisibleEntity(0, &m_DummyEnt);
	gEngfuncs.CL_CreateVisibleEntity(0, &m_DummyTransEnt);

	m_bInitDone = true;
}

void CTempEntManager::InitTempEnts()
{
	memset(m_TempEnts, 0, sizeof(TEMPENTITY) * MAX_TEMPENTS);

	for (size_t i = 0; i < MAX_TEMPENTS; i++)
	{
		m_TempEnts[i].next = &m_TempEnts[i + 1];
	}
	m_TempEnts[MAX_TEMPENTS - 1].next = nullptr;

	m_bInitDone = false;
}