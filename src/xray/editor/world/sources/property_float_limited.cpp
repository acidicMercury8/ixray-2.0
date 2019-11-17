////////////////////////////////////////////////////////////////////////////
//	Created 	: 07.12.2007
//	Author		: Dmitriy Iassenev
//	Copyright (C) GSC Game World - 2009
////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "property_float_limited.h"

property_float_limited::property_float_limited			(
		float_getter_type^ getter,
		float_setter_type^ setter,
		float const% increment_factor,
		float const %min,
		float const %max
	) :
	inherited				(getter, setter, increment_factor),
	m_min					(min),
	m_max					(max)
{
}

System::Object ^property_float_limited::get_value		()
{
	float					value = safe_cast<float>(inherited::get_value());
	if (value < m_min)
		value				= m_min;

	if (value > m_max)
		value				= m_max;

	return					(value);
}

void property_float_limited::set_value			(System::Object ^object)
{
	float					new_value = safe_cast<float>(object);

	if (new_value < m_min)
		new_value			= m_min;

	if (new_value > m_max)
		new_value			= m_max;

	inherited::set_value	(new_value);
}