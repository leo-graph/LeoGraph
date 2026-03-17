//
// EntityResolverImpl.cpp
//
// Library: XML
// Package: SAX
// Module:  SAX
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//

#include "Poco/SAX/EntityResolverImpl.h"
#include "Poco/Exception.h"
#include "Poco/Path.h"
#include "Poco/SAX/InputSource.h"
#include "Poco/URI.h"
#include "Poco/XML/XMLString.h"

using Poco::Exception;
using Poco::IOException;
using Poco::OpenFileException;
using Poco::Path;
using Poco::URI;
using Poco::URIStreamOpener;

namespace Poco {
namespace XML {

EntityResolverImpl::EntityResolverImpl() : _opener(URIStreamOpener::defaultOpener()) {}

EntityResolverImpl::EntityResolverImpl(const URIStreamOpener& opener) : _opener(opener) {}

EntityResolverImpl::~EntityResolverImpl() {}

InputSource* EntityResolverImpl::resolveEntity(const XMLString* publicId, const XMLString& systemId) {
  std::istream* pIstr = resolveSystemId(systemId);
  InputSource* pInputSource = new InputSource(systemId);
  if (publicId) pInputSource->setPublicId(*publicId);
  pInputSource->setByteStream(*pIstr);
  return pInputSource;
}

void EntityResolverImpl::releaseInputSource(InputSource* pSource) {
  poco_check_ptr(pSource);

  delete pSource->getByteStream();
  delete pSource;
}

std::istream* EntityResolverImpl::resolveSystemId(const XMLString& systemId) {
  std::string sid = fromXMLString(systemId);
  return _opener.open(sid);
}

}  // namespace XML
}  // namespace Poco
