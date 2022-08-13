// SPDX-License-Identifier: LGPL-2.1-only
// Copyright (C) 2022 James Hogan <james@albanarts.com>

#ifndef OSGXR_EXTENSION
#define OSGXR_EXTENSION 1

#include <osgXR/Extension>

#include <set>
#include <string>

namespace osgXR {

class XRState;

namespace OpenXR {
    class Instance;
}

class Extension::Private
{
    public:

        static std::shared_ptr<Private> get(Extension *pub)
        {
            if (pub)
                return pub->_private;
            else
                return nullptr;
        }

        static const std::shared_ptr<Private> get(const Extension *pub)
        {
            if (pub)
                return pub->_private;
            else
                return nullptr;
        }

        Private(XRState *state, const std::string &name);

        // Public object registration
        void registerPublic(Extension *extension);
        void unregisterPublic(Extension *extension);

        // Dependencies

        bool getDependsOn(const std::shared_ptr<Private> &extension) const;
        void addDependency(std::shared_ptr<Private> &dependency);

        // Accessors

        const std::string &getName() const
        {
            return _name;
        }

        bool getAvailable() const;
        bool getAvailable(uint32_t *outVersion) const;
        uint32_t getVersion() const;
        bool getEnabled() const;

        // Internal

        /**
         * Enable this extension and any dependencies if possible.
         * Only to be called if the extension and all dependencies are
         * available.
         * @param instance OpenXR instance object.
         */
        void setup(OpenXR::Instance *instance);

        /// Clean up after a removed instance.
        void cleanup();

    private:

        void notifyChanged() const;

        void probe() const;

        XRState *_state;
        std::string _name;
        std::set<Extension *> _publics;
        std::set<std::shared_ptr<Private>> _dependencies;
        bool _enabled;

        // cache
        mutable bool _probed;
        mutable bool _available;
        mutable uint32_t _version;
};

} // osgXR

#endif
