/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#include <et/core/sequence.h>

using namespace et;

Sequence::Sequence(float duration, float from, float to, Curve curve) : 
	_curve(curve), _duration(duration), _from(from), _to(to), _dt(to - from)
{

}

void Sequence::start(TimerPool::Pointer tp)
{
	startUpdates(tp.pointer());

	_startTime = actualTime();
	_endTime = _startTime + _duration;
}

void Sequence::update(float t)
{
	if (t >= _endTime)
	{
		cancelUpdates();
		updated.invoke(this, _to);
		finished.invoke(this);
	}
	else
	{
		float normalizedTime = (t - _startTime) / _duration;
		if (_curve == Curve::EasyIn)
			normalizedTime = ::sqrtf(normalizedTime);
		else if (_curve == Curve::EasyOut)
			normalizedTime *= normalizedTime;
		updated.invoke(this, _from + _dt * normalizedTime);
	}
}
