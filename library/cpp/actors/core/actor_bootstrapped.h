#pragma once

#include "actorsystem.h"
#include "actor.h"
#include "events.h"
#include <util/generic/noncopyable.h>

namespace NActors {
    template<typename T> struct dependent_false : std::false_type {};

    template<typename TDerived>
    class TActorBootstrapped: public TActor<TDerived> {
    protected:
        TAutoPtr<IEventHandle> AfterRegister(const TActorId& self, const TActorId& parentId) override {
            return new IEventHandleFat(TEvents::TSystem::Bootstrap, 0, self, parentId, {}, 0);
        }

        STFUNC(StateBootstrap) {
            Y_VERIFY(ev->GetTypeRewrite() == TEvents::TSystem::Bootstrap, "Unexpected bootstrap message");
            using T = decltype(&TDerived::Bootstrap);
            TDerived& self = static_cast<TDerived&>(*this);
            if constexpr (std::is_invocable_v<T, TDerived, const TActorContext&>) {
                self.Bootstrap(ctx);
            } else if constexpr (std::is_invocable_v<T, TDerived, const TActorId&, const TActorContext&>) {
                self.Bootstrap(ev->Sender, ctx);
            } else if constexpr (std::is_invocable_v<T, TDerived>) {
                self.Bootstrap();
            } else if constexpr (std::is_invocable_v<T, TDerived, const TActorId&>) {
                self.Bootstrap(ev->Sender);
            } else {
                static_assert(dependent_false<TDerived>::value, "No correct Bootstrap() signature");
            }
        }

        TActorBootstrapped()
            : TActor<TDerived>(&TDerived::StateBootstrap) {
        }
    };
}
