// COPYING: Copyright 2017-2020 Tyler Gilbert and Stratify Labs. All rights
// reserved
#ifndef FRAMEWORK_HPP
#define FRAMEWORK_HPP

#include "Group.hpp"

/*!
 * \brief Framework Class
 * \details The Framework Class is used to publish
 * and download a framework that can be used to develop
 * on custom hardware.
 *
 * Publishing frameworks require an enterprise account.
 *
 */
class Framework : public Group {
public:
  Framework();
};

#endif // FRAMEWORK_HPP
