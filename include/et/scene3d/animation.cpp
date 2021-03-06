/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#include <et/core/serialization.h>
#include <et/scene3d/animation.h>

using namespace et;
using namespace et::s3d;

Animation::Animation()
{
}

Animation::Animation(Dictionary)
{
}

void Animation::addKeyFrame(float t, const vec3& tr, const quaternion& o, const vec3& s)
{
	_frames.emplace_back(t, tr, o, s);
}

void Animation::setTimeRange(float start, float stop)
{
	ET_ASSERT(stop - start >= 0.0f);
	_startTime = start;
	_stopTime = stop;
}

void Animation::setFrameRate(float r)
{
	_frameRate = r;
}

void Animation::transformation(float time, vec3& t, quaternion& o, vec3& s) const
{
	float d = duration();
	
	if (d == 0.0f)
	{
		auto mainTransform = _frames.front();
		t = mainTransform.translation;
		o = mainTransform.orientation;
		s = mainTransform.scale;
		return;
	}
	
	switch (_outOfRangeMode)
	{
		case OutOfRangeMode_Loop:
		{
			float timeAspect = time / d;
			time = _startTime + d * (timeAspect - std::floor(timeAspect));
			break;
		}
			
		case OutOfRangeMode_PingPong:
		{
			float timeAspect = time / d;
			time = _startTime + d * (1.0f - 2.0f * std::abs(0.5f * (timeAspect - 1.0f) - std::floor(0.5f * timeAspect)));
			break;
		}
			
		default:
			break;
	}
	
	time = clamp(time, _startTime, _stopTime);
	
	int nearestLowerFrame = static_cast<int32_t>(_frames.size()) - 1;
	for (auto i = _frames.rbegin(), e = _frames.rend(); (i != e) && (nearestLowerFrame >= 0); ++i)
	{
		if (i->time <= time) break;
		--nearestLowerFrame;
	}
	
	if (static_cast<size_t>(nearestLowerFrame + 1) >= _frames.size())
	{
		const auto& frame = _frames.at(nearestLowerFrame);
		t = frame.translation;
		o = frame.orientation;
		s = frame.scale;
	}
	else
	{
		const auto& lowerFrame = _frames.at(nearestLowerFrame);
		const auto& upperFrame = _frames.at(nearestLowerFrame + 1);
		float interolationFactor = (time - lowerFrame.time) / (upperFrame.time - lowerFrame.time);
		t = mix(lowerFrame.translation, upperFrame.translation, interolationFactor);
		o = slerp(lowerFrame.orientation, upperFrame.orientation, interolationFactor);
		s = mix(lowerFrame.scale, upperFrame.scale, interolationFactor);
	}
}

mat4 Animation::transformation(float time) const
{
	vec3 t(0.0f);
	vec3 s(0.0f);
	quaternion o;

	transformation(time, t, o, s);
	
	ComponentTransformable result;
	result.setTranslation(t);
	result.setOrientation(o);
	result.setScale(s);
	return result.transform();
}

void Animation::setOutOfRangeMode(OutOfRangeMode mode)
{
	_outOfRangeMode = mode;
}
