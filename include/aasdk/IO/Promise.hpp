// This file is part of aasdk library project.
// Copyright (C) 2018 f1x.studio (Michal Szwaj)
// Copyright (C) 2024 CubeOne (Simon Dean - simon.dean@cubeone.co.uk)
//
// aasdk is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// aasdk is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with aasdk. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <functional>
#include <boost/asio.hpp>
#include <boost/core/noncopyable.hpp>
#include <Error/Error.hpp>
#include <IO/IOContextWrapper.hpp>


namespace aasdk {
  namespace io {
      using IoContext = boost::asio::io_context;
      using Strand = boost::asio::strand<IoContext::executor_type>;


    template<typename ResolveArgumentType, typename ErrorArgumentType = error::Error>
    class Promise : boost::noncopyable {
    public:
      typedef ResolveArgumentType ValueType;
      typedef ErrorArgumentType ErrorType;
      typedef std::function<void(ResolveArgumentType)> ResolveHandler;
      typedef std::function<void(ErrorArgumentType)> RejectHandler;
      typedef std::shared_ptr<Promise> Pointer;

      static Pointer defer(IoContext &ioContext) {
        // Corrected make_shared
        return std::make_shared<Promise<ResolveArgumentType, ErrorArgumentType>>(ioContext);
      }

      static Pointer defer(Strand &strand) {
        // Corrected make_shared
        return std::make_shared<Promise<ResolveArgumentType, ErrorArgumentType>>(strand);
      }

      Promise(IoContext &ioContext)
          : ioContextWrapper_(ioContext) {

      }

      Promise(Strand &strand)
          : ioContextWrapper_(strand) {

      }

      void then(ResolveHandler resolveHandler, RejectHandler rejectHandler = RejectHandler()) {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        resolveHandler_ = std::move(resolveHandler);
        rejectHandler_ = std::move(rejectHandler);
      }

      void resolve(ResolveArgumentType argument) {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        if (resolveHandler_ != nullptr && this->isPending()) {
          ioContextWrapper_.post(
              [argument = std::move(argument), resolveHandler = std::move(resolveHandler_)]() mutable {
                resolveHandler(std::move(argument));
              });
        }

        ioContextWrapper_.reset();
        rejectHandler_ = RejectHandler();
      }

      void reject(ErrorArgumentType error) {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        if (rejectHandler_ != nullptr && this->isPending()) {
          ioContextWrapper_.post([error = std::move(error), rejectHandler = std::move(rejectHandler_)]() mutable {
            rejectHandler(std::move(error));
          });
        }

        ioContextWrapper_.reset();
        resolveHandler_ = ResolveHandler();
      }

    private:
      bool isPending() const {
        return ioContextWrapper_.isActive();
      }

      ResolveHandler resolveHandler_;
      RejectHandler rejectHandler_;
      IOContextWrapper ioContextWrapper_;
      std::mutex mutex_;
    };

    template<typename ErrorArgumentType>
    class Promise<void, ErrorArgumentType> : boost::noncopyable {
    public:
      typedef ErrorArgumentType ErrorType;
      typedef std::function<void()> ResolveHandler;
      typedef std::function<void(ErrorArgumentType)> RejectHandler;
      typedef std::shared_ptr<Promise> Pointer;

      static Pointer defer(IoContext &ioContext) {
        // Corrected make_shared
        return std::make_shared<Promise<void, ErrorArgumentType>>(ioContext);
      }

      static Pointer defer(Strand &strand) {
        // Corrected make_shared
        return std::make_shared<Promise<void, ErrorArgumentType>>(strand);
      }

      Promise(IoContext &ioContext)
          : ioContextWrapper_(ioContext) {

      }

      Promise(Strand &strand)
          : ioContextWrapper_(strand) {

      }

      void then(ResolveHandler resolveHandler, RejectHandler rejectHandler = RejectHandler()) {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        resolveHandler_ = std::move(resolveHandler);
        rejectHandler_ = std::move(rejectHandler);
      }

      void resolve() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        if (resolveHandler_ != nullptr && this->isPending()) {
          ioContextWrapper_.post([resolveHandler = std::move(resolveHandler_)]() mutable {
            resolveHandler();
          });
        }

        ioContextWrapper_.reset();
        rejectHandler_ = RejectHandler();
      }

      void reject(ErrorArgumentType error) {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        if (rejectHandler_ != nullptr && this->isPending()) {
          ioContextWrapper_.post([error = std::move(error), rejectHandler = std::move(rejectHandler_)]() mutable {
            rejectHandler(std::move(error));
          });
        }

        ioContextWrapper_.reset();
        resolveHandler_ = ResolveHandler();
      }

    private:
      bool isPending() const {
        return ioContextWrapper_.isActive();
      }

      ResolveHandler resolveHandler_;
      RejectHandler rejectHandler_;
      IOContextWrapper ioContextWrapper_;
      std::mutex mutex_;
    };

    template<>
    class Promise<void, void> : boost::noncopyable {
    public:
      typedef std::function<void()> ResolveHandler;
      typedef std::function<void()> RejectHandler;
      typedef std::shared_ptr<Promise> Pointer;

      static Pointer defer(IoContext &ioContext) {
        return std::make_shared<Promise<void, void>>(ioContext);
      }

      static Pointer defer(Strand &strand) {
        return std::make_shared<Promise<void, void>>(strand);
      }



      Promise(IoContext &ioContext)
          : ioContextWrapper_(ioContext) {

      }

      Promise(Strand &strand)
          : ioContextWrapper_(strand) {

      }

      void then(ResolveHandler resolveHandler, RejectHandler rejectHandler = RejectHandler()) {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        resolveHandler_ = std::move(resolveHandler);
        rejectHandler_ = std::move(rejectHandler);
      }

      void resolve() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        if (resolveHandler_ != nullptr && this->isPending()) {
          ioContextWrapper_.post([resolveHandler = std::move(resolveHandler_)]() mutable {
            resolveHandler();
          });
        }

        ioContextWrapper_.reset();
        rejectHandler_ = RejectHandler();
      }

      void reject() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        if (rejectHandler_ != nullptr && this->isPending()) {
          ioContextWrapper_.post([rejectHandler = std::move(rejectHandler_)]() mutable {
            rejectHandler();
          });
        }

        ioContextWrapper_.reset();
        resolveHandler_ = ResolveHandler();
      }

    private:
      bool isPending() const {
        return ioContextWrapper_.isActive();
      }

      ResolveHandler resolveHandler_;
      RejectHandler rejectHandler_;
      IOContextWrapper ioContextWrapper_;
      std::mutex mutex_;
    };

    template<typename ResolveArgumentType>
    class Promise<ResolveArgumentType, void> : boost::noncopyable {
    public:
      typedef ResolveArgumentType ValueType;
      typedef std::function<void(ResolveArgumentType)> ResolveHandler;
      typedef std::function<void(void)> RejectHandler;
      typedef std::shared_ptr<Promise> Pointer;

      static Pointer defer(IoContext &ioContext) {
        // Corrected make_shared
        return std::make_shared<Promise<ResolveArgumentType, void>>(ioContext);
      }

      static Pointer defer(Strand &strand) {
        // Corrected make_shared
        return std::make_shared<Promise<ResolveArgumentType, void>>(strand);
      }

      Promise(IoContext &ioContext)
          : ioContextWrapper_(ioContext) {

      }

      Promise(Strand &strand)
          : ioContextWrapper_(strand) {

      }

      void then(ResolveHandler resolveHandler, RejectHandler rejectHandler = RejectHandler()) {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        resolveHandler_ = std::move(resolveHandler);
        rejectHandler_ = std::move(rejectHandler);
      }

      void resolve(ResolveArgumentType argument) {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        if (resolveHandler_ != nullptr && this->isPending()) {
          ioContextWrapper_.post(
              [argument = std::move(argument), resolveHandler = std::move(resolveHandler_)]() mutable {
                resolveHandler(std::move(argument));
              });
        }

        ioContextWrapper_.reset();
        rejectHandler_ = RejectHandler();
      }

      void reject() {
        std::lock_guard<decltype(mutex_)> lock(mutex_);

        if (rejectHandler_ != nullptr && this->isPending()) {
          ioContextWrapper_.post([rejectHandler = std::move(rejectHandler_)]() mutable {
            rejectHandler();
          });
        }

        ioContextWrapper_.reset();
        resolveHandler_ = ResolveHandler();
      }

    private:
      bool isPending() const {
        return ioContextWrapper_.isActive();
      }

      ResolveHandler resolveHandler_;
      RejectHandler rejectHandler_;
      IOContextWrapper ioContextWrapper_;
      std::mutex mutex_;
    };


  }
}
