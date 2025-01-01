// BlueNightHawk - 2025
// Reference : Xash3D FWGS

#include "cl_dll.h"
#include "PlatformHeaders.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_opengl.h"

#include <algorithm>

#include "studio.h"
#include "com_model.h"
#include "r_studioint.h"

#include "pmtrace.h"
#include "pm_defs.h"

#include "event_api.h"
#include "triangleapi.h"

#include "cl_tempents.h"

#define GLARE_FALLOFF 19000.0f

#define VectorSet(v, x, y, z) ((v)[0] = (x), (v)[1] = (y), (v)[2] = (z))
static void SinCos(float radians, float* sine, float* cosine)
{
	*sine = sin(radians);
	*cosine = cos(radians);
}

PFNGLACTIVETEXTUREPROC pglActiveTexture;

/*
================
GetGlProcAddress
================
*/
static void* GetGlProcAddress(const char* proc)
{
#ifdef WIN32
	return wglGetProcAddress(proc);
#else
	return SDL_GL_GetProcAddress(proc);
#endif
}

/*
================
R_GlowSightDistance

Set sprite brightness factor
================
*/
static float R_SpriteGlowBlend(Vector origin, int rendermode, int renderfx, float* pscale)
{
	float dist, brightness;
	Vector glowDist;
	pmtrace_t tr;

	VectorSubtract(origin, g_TempEntMan.refdef.vieworg, glowDist);
	dist = Length(glowDist);

	// if (RP_NORMALPASS())
	{
		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);

		gEngfuncs.pEventAPI->EV_PushPMStates();
		gEngfuncs.pEventAPI->EV_SetSolidPlayers(gEngfuncs.GetLocalPlayer()->index - 1);

		gEngfuncs.pEventAPI->EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(g_TempEntMan.refdef.vieworg, origin, /* r_traceglow.value ? PM_GLASS_IGNORE :*/ (PM_GLASS_IGNORE | PM_STUDIO_IGNORE), -1, &tr);

		gEngfuncs.pEventAPI->EV_PopPMStates();

		if ((1.0f - tr.fraction) * dist > 8.0f)
			return 0.0f;
	}

	if (renderfx == kRenderFxNoDissipation)
		return 1.0f;

	brightness = GLARE_FALLOFF / (dist * dist);
	brightness = std::clamp(brightness, 0.05f, 1.0f);
	*pscale *= dist * (1.0f / 200.0f);

	return brightness;
}

/*
================
R_SpriteOccluded

Do occlusion test for glow-sprites
================
*/
static bool R_SpriteOccluded(cl_entity_s* pEntity, float* pblend, float* scale)
{
	float blend = 0.0f;
	Vector sprite_mins, sprite_maxs;
	auto model = pEntity->model;
	auto origin = pEntity->origin;

	// scale original bbox (no rotation for sprites)
	VectorScale(model->mins, *scale, sprite_mins);
	VectorScale(model->maxs, *scale, sprite_maxs);

	VectorAdd(sprite_mins, origin, sprite_mins);
	VectorAdd(sprite_maxs, origin, sprite_maxs);

	if (pEntity->curstate.rendermode == kRenderGlow)
	{
		if (!gEngfuncs.pTriAPI->BoxInPVS(sprite_mins, sprite_maxs))
		{
			return true;
		}

		blend = R_SpriteGlowBlend(origin, pEntity->curstate.rendermode, pEntity->curstate.renderfx, scale);
		*pblend *= blend;

		if (blend <= 0.01f)
			return true; // faded
	}

	return !gEngfuncs.pTriAPI->BoxInPVS(sprite_mins, sprite_maxs);
}

/*
================
R_GetSpriteType
================
*/
static int R_GetSpriteType(cl_entity_s* pEntity)
{
	if (!pEntity->model->cache.data)
		return -1;

	return ((msprite_t*)pEntity->model->cache.data)->type;
}


/*
================
R_SetSpriteOrientation


================
*/
static void R_SetSpriteOrientation(cl_entity_s* pEntity, Vector& origin, Vector& v_forward, Vector& v_right, Vector& v_up)
{
	float angle, dot, sr, cr;
	auto type = R_GetSpriteType(pEntity);
	Vector angles = pEntity->angles;

	origin = pEntity->origin;

	// automatically roll parallel sprites if requested
	if (pEntity->angles[2] != 0.0f && type == SPR_VP_PARALLEL)
		type = SPR_VP_PARALLEL_ORIENTED;

	switch (type)
	{
	case SPR_ORIENTED:
		gEngfuncs.pfnAngleVectors(pEntity->angles, v_forward, v_right, v_up);
		VectorScale(v_forward, 0.01f, v_forward); // to avoid z-fighting
		VectorSubtract(origin, v_forward, origin);
		break;
	case SPR_FACING_UPRIGHT:
		VectorSet(v_right, origin[1] - g_TempEntMan.refdef.vieworg[1], -(origin[0] - g_TempEntMan.refdef.vieworg[0]), 0.0f);
		VectorSet(v_up, 0.0f, 0.0f, 1.0f);
		VectorNormalize(v_right);
		break;
	case SPR_VP_PARALLEL_UPRIGHT:
		dot = g_TempEntMan.refdef.forward[2];
		if ((dot > 0.999848f) || (dot < -0.999848f)) // cos(1 degree) = 0.999848
			return;									 // invisible
		VectorSet(v_up, 0.0f, 0.0f, 1.0f);
		VectorSet(v_right, g_TempEntMan.refdef.forward[1], -g_TempEntMan.refdef.forward[0], 0.0f);
		VectorNormalize(v_right);
		break;
	case SPR_VP_PARALLEL_ORIENTED:
		angle = pEntity->angles[2] * (M_PI / 360.0f);
		SinCos(angle, &sr, &cr);
		for (int i = 0; i < 3; i++)
		{
			v_right[i] = (g_TempEntMan.refdef.right[i] * cr + g_TempEntMan.refdef.up[i] * sr);
			v_up[i] = g_TempEntMan.refdef.right[i] * -sr + g_TempEntMan.refdef.up[i] * cr;
		}
		break;
	case SPR_VP_PARALLEL: // normal sprite
	default:
		VectorCopy(g_TempEntMan.refdef.right, v_right);
		VectorCopy(g_TempEntMan.refdef.up, v_up);
		break;
	}
}


/*
================
R_SetRenderMode

Set render mode for sprites
================
*/
static void R_SetRenderMode(int rendermode)
{
	gEngfuncs.pTriAPI->RenderMode(rendermode);

	// select properly rendermode
	switch (rendermode)
	{
	case kRenderTransAlpha:
		glDepthMask(GL_FALSE);
		// fallthrough
	case kRenderTransColor:
	case kRenderTransTexture:
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case kRenderGlow:
		glDisable(GL_DEPTH_TEST);
		// fallthrough
	case kRenderTransAdd:
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glDepthMask(GL_FALSE);
		break;
	case kRenderNormal:
	default:
		glDisable(GL_BLEND);
		break;
	}

	// all sprites can have color
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_ALPHA_TEST);
}

/*
===============
CL_FxBlend
===============
*/
static int CL_FxBlend(cl_entity_t* e)
{
	int blend = 0;
	float offset, dist;
	Vector tmp;

	auto pl = gEngfuncs.GetLocalPlayer();

	Vector vieworg = g_TempEntMan.refdef.vieworg;
	Vector viewangles, forward, right, up;

	gEngfuncs.pfnAngleVectors(g_TempEntMan.refdef.viewangles, forward, right, up);

	offset = ((int)e->index) * 363.0f; // Use ent index to de-sync these fx

	switch (e->curstate.renderfx)
	{
	case kRenderFxPulseSlowWide:
		blend = e->curstate.renderamt + 0x40 * sin(gEngfuncs.GetClientTime() * 2 + offset);
		break;
	case kRenderFxPulseFastWide:
		blend = e->curstate.renderamt + 0x40 * sin(gEngfuncs.GetClientTime() * 8 + offset);
		break;
	case kRenderFxPulseSlow:
		blend = e->curstate.renderamt + 0x10 * sin(gEngfuncs.GetClientTime() * 2 + offset);
		break;
	case kRenderFxPulseFast:
		blend = e->curstate.renderamt + 0x10 * sin(gEngfuncs.GetClientTime() * 8 + offset);
		break;
	case kRenderFxFadeSlow:
		// if (RP_NORMALPASS())
		{
			if (e->curstate.renderamt > 0)
				e->curstate.renderamt -= 1;
			else
				e->curstate.renderamt = 0;
		}
		blend = e->curstate.renderamt;
		break;
	case kRenderFxFadeFast:
		// if (RP_NORMALPASS())
		{
			if (e->curstate.renderamt > 3)
				e->curstate.renderamt -= 4;
			else
				e->curstate.renderamt = 0;
		}
		blend = e->curstate.renderamt;
		break;
	case kRenderFxSolidSlow:
		// if (RP_NORMALPASS())
		{
			if (e->curstate.renderamt < 255)
				e->curstate.renderamt += 1;
			else
				e->curstate.renderamt = 255;
		}
		blend = e->curstate.renderamt;
		break;
	case kRenderFxSolidFast:
		// if (RP_NORMALPASS())
		{
			if (e->curstate.renderamt < 252)
				e->curstate.renderamt += 4;
			else
				e->curstate.renderamt = 255;
		}
		blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeSlow:
		blend = 20 * sin(gEngfuncs.GetClientTime() * 4 + offset);
		if (blend < 0)
			blend = 0;
		else
			blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeFast:
		blend = 20 * sin(gEngfuncs.GetClientTime() * 16 + offset);
		if (blend < 0)
			blend = 0;
		else
			blend = e->curstate.renderamt;
		break;
	case kRenderFxStrobeFaster:
		blend = 20 * sin(gEngfuncs.GetClientTime() * 36 + offset);
		if (blend < 0)
			blend = 0;
		else
			blend = e->curstate.renderamt;
		break;
	case kRenderFxFlickerSlow:
		blend = 20 * (sin(gEngfuncs.GetClientTime() * 2) + sin(gEngfuncs.GetClientTime() * 17 + offset));
		if (blend < 0)
			blend = 0;
		else
			blend = e->curstate.renderamt;
		break;
	case kRenderFxFlickerFast:
		blend = 20 * (sin(gEngfuncs.GetClientTime() * 16) + sin(gEngfuncs.GetClientTime() * 23 + offset));
		if (blend < 0)
			blend = 0;
		else
			blend = e->curstate.renderamt;
		break;
	case kRenderFxHologram:
	case kRenderFxDistort:
		VectorCopy(e->origin, tmp);
		VectorSubtract(tmp, vieworg, tmp);
		dist = DotProduct(tmp, forward);

		// turn off distance fade
		if (e->curstate.renderfx == kRenderFxDistort)
			dist = 1;

		if (dist <= 0)
		{
			blend = 0;
		}
		else
		{
			e->curstate.renderamt = 180;
			if (dist <= 100)
				blend = e->curstate.renderamt;
			else
				blend = (int)((1.0f - (dist - 100) * (1.0f / 400.0f)) * e->curstate.renderamt);
			blend += gEngfuncs.pfnRandomLong(-32, 31);
		}
		break;
	default:
		blend = e->curstate.renderamt;
		break;
	}

	blend = std::clamp(blend, 0, 255);

	return blend;
}


/*
================
R_GetSpriteFrame
================
*/
static mspriteframe_t* R_GetSpriteFrame(cl_entity_t* currententity)
{
	msprite_t* psprite;
	mspritegroup_t* pspritegroup;
	mspriteframe_t* pspriteframe;
	int i, numframes, frame;
	float *pintervals, fullinterval, targettime, time;

	psprite = (msprite_t*)currententity->model->cache.data;
	frame = currententity->curstate.frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		// gEngfuncs.Con_Printf("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}
	else if (frame >= psprite->numframes)
	{
	//	if (frame > psprite->numframes)
	//		gEngfuncs.Con_Printf(S_WARN "%s: no such frame %d (%s)\n", __func__, frame, pModel->name);
		frame = psprite->numframes - 1;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t*)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes - 1];

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		targettime = gEngfuncs.GetClientTime() - ((int)(gEngfuncs.GetClientTime() / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++)
		{
			if (pintervals[i] > targettime)
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

/*
=================
R_DrawSpriteQuad
=================
*/
static void R_DrawSpriteQuad(const mspriteframe_t* frame, const Vector org, const Vector v_right, const Vector v_up, const float scale)
{
	Vector point;

	glTexCoord2f(0.0f, 1.0f);
	VectorMA(org, frame->down * scale, v_up, point);
	VectorMA(point, frame->left * scale, v_right, point);
	glVertex3fv(point);
	glTexCoord2f(0.0f, 0.0f);
	VectorMA(org, frame->up * scale, v_up, point);
	VectorMA(point, frame->left * scale, v_right, point);
	glVertex3fv(point);
	glTexCoord2f(1.0f, 0.0f);
	VectorMA(org, frame->up * scale, v_up, point);
	VectorMA(point, frame->right * scale, v_right, point);
	glVertex3fv(point);
	glTexCoord2f(1.0f, 1.0f);
	VectorMA(org, frame->down * scale, v_up, point);
	VectorMA(point, frame->right * scale, v_right, point);
	glVertex3fv(point);
}


/*
================
CL_DrawSpriteTempEnt

Setup and draw sprite tempent
================
*/
static void CL_DrawSpriteTempEnt(cl_entity_s* pEntity)
{
	Vector vColor = Vector(pEntity->curstate.rendercolor.r / 255.0f, pEntity->curstate.rendercolor.g / 255.0f, pEntity->curstate.rendercolor.b / 255.0f);
	float blend = CL_FxBlend(pEntity) / 255.0f;
	float scale = pEntity->curstate.scale;

	Vector origin, forward, right, up;

	R_SetSpriteOrientation(pEntity, origin, forward, right, up);

	if (scale <= 0.0f)
		scale = 1.0f;

	if (R_SpriteOccluded(pEntity, &blend, &scale))
	{
		return;
	}

	if (blend <= 0.001f)
		return;

	// NOTE: never pass sprites with rendercolor '0 0 0' it's a stupid Valve Hammer Editor bug
	if (pEntity->curstate.rendercolor.r || pEntity->curstate.rendercolor.g || pEntity->curstate.rendercolor.b)
	{
		vColor[0] = (float)pEntity->curstate.rendercolor.r * (1.0f / 255.0f);
		vColor[1] = (float)pEntity->curstate.rendercolor.g * (1.0f / 255.0f);
		vColor[2] = (float)pEntity->curstate.rendercolor.b * (1.0f / 255.0f);
	}
	else
	{
		vColor[0] = 1.0f;
		vColor[1] = 1.0f;
		vColor[2] = 1.0f;
	}

	R_SetRenderMode(pEntity->curstate.rendermode);

	gEngfuncs.pTriAPI->CullFace(TRI_NONE);

	auto sprframe = R_GetSpriteFrame(pEntity);
	glBindTexture(GL_TEXTURE_2D, sprframe->gl_texturenum);

	glBegin(GL_QUADS);
	glColor4f(vColor.x, vColor.y, vColor.z,
		blend);

	R_DrawSpriteQuad(sprframe, origin, right, up, scale);

	glEnd();

	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
	gEngfuncs.pTriAPI->CullFace(TRI_FRONT);

	glDisable(GL_ALPHA_TEST);
	glDepthMask(GL_TRUE);

	if (pEntity->curstate.rendermode != kRenderNormal)
	{
		glDisable(GL_BLEND);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_DEPTH_TEST);
	}
}

/*
================
DrawSpriteTempEnts
================
*/
void CTempEntManager::DrawSpriteTempEnts()
{
	if (m_TransVisEnts.size() == 0 && m_VisEnts.size() == 0)
		return;

	if (!pglActiveTexture)
		pglActiveTexture = decltype(pglActiveTexture)(GetGlProcAddress("glActiveTexture"));

	// BUzer: workaround half-life's bug, when multitexturing left enabled after
	// rendering brush entities
	pglActiveTexture(GL_TEXTURE1);
	glDisable(GL_TEXTURE_2D);

	// Set the active texture unit
	pglActiveTexture(GL_TEXTURE0);
	glPushAttrib(GL_TEXTURE_BIT);
	glEnable(GL_TEXTURE_2D);

	glPushAttrib(GL_TEXTURE_BIT | GL_DEPTH_BUFFER_BIT);

	for (auto f : m_VisEnts)
	{
		if (f->model->type == mod_sprite)
		{
			CL_DrawSpriteTempEnt(f);
			f->prevstate = f->curstate;
		}
	}

	for (auto f : m_TransVisEnts)
	{
		if (f->model->type == mod_sprite)
		{
			CL_DrawSpriteTempEnt(f);
			f->prevstate = f->curstate;
		}
	}

	glPopAttrib();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

	m_TransVisEnts.clear();
	m_VisEnts.clear();
}
