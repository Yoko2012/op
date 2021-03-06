/*

 Copyright (c) 2013, SMB Phone Inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 The views and conclusions contained in the software and documentation are those
 of the authors and should not be interpreted as representing official policies,
 either expressed or implied, of the FreeBSD Project.

 */

#include <openpeer/core/internal/core_Account.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_Identity.h>
#include <openpeer/core/internal/core_Contact.h>
#include <openpeer/core/internal/core_ConversationThread.h>
#include <openpeer/core/internal/core_Helper.h>
#include <openpeer/stack/IBootstrappedNetwork.h>
#include <openpeer/stack/IPeer.h>
#include <openpeer/stack/IPeerFiles.h>
#include <openpeer/stack/IServiceLockbox.h>
#include <openpeer/stack/IPeerFilePrivate.h>
#include <openpeer/stack/IPublication.h>
#include <openpeer/stack/IPublicationRepository.h>
#include <openpeer/stack/message/IMessageHelper.h>

#include <openpeer/services/IHTTP.h>

#include <zsLib/Stringize.h>
#include <zsLib/helpers.h>
#include <zsLib/XML.h>

#define OPENPEER_PEER_SUBSCRIPTION_AUTO_CLOSE_TIMEOUT_IN_SECONDS (60*3)

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_core) } }


namespace openpeer
{
  namespace core
  {
    namespace internal
    {
      using zsLib::string;
      typedef zsLib::XML::Exceptions::CheckFailed CheckFailed;

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForCall
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForContact
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark IAccountForConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account
      #pragma mark

      //-----------------------------------------------------------------------
      Account::Account(
                       IMessageQueuePtr queue,
                       IAccountDelegatePtr delegate,
                       IConversationThreadDelegatePtr conversationThreadDelegate,
                       ICallDelegatePtr callDelegate
                       ) :
        MessageQueueAssociator(queue),
        mID(zsLib::createPUID()),
        mDelegate(IAccountDelegateProxy::createWeak(IStackForInternal::queueApplication(), delegate)),
        mConversationThreadDelegate(IConversationThreadDelegateProxy::createWeak(IStackForInternal::queueApplication(), conversationThreadDelegate)),
        mCallDelegate(ICallDelegateProxy::createWeak(IStackForInternal::queueApplication(), callDelegate)),
        mCurrentState(AccountState_Pending),
        mLastErrorCode(0),
        mLockboxForceCreateNewAccount(false)
      {
        ZS_LOG_BASIC(log("created"))
      }

      //-----------------------------------------------------------------------
      Account::~Account()
      {
        if(isNoop()) return;
        
        mThisWeak.reset();
        ZS_LOG_BASIC(log("destroyed"))
        cancel();
      }

      //-----------------------------------------------------------------------
      void Account::init()
      {
        AutoRecursiveLock lock(mLock);
        step();
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::convert(IAccountPtr account)
      {
        return boost::dynamic_pointer_cast<Account>(account);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccount
      #pragma mark

      //-----------------------------------------------------------------------
      String Account::toDebugString(IAccountPtr account, bool includeCommaPrefix)
      {
        if (!account) return includeCommaPrefix ? String(", account=(null)") : String("account=(null)");
        return Account::convert(account)->getDebugValueString(includeCommaPrefix);
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::login(
                                IAccountDelegatePtr delegate,
                                IConversationThreadDelegatePtr conversationThreadDelegate,
                                ICallDelegatePtr callDelegate,
                                const char *namespaceGrantOuterFrameURLUponReload,
                                const char *grantID,
                                const char *lockboxServiceDomain,
                                bool forceCreateNewLockboxAccount
                                )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!namespaceGrantOuterFrameURLUponReload)
        ZS_THROW_INVALID_ARGUMENT_IF(!grantID)
        ZS_THROW_INVALID_ARGUMENT_IF(!lockboxServiceDomain)

        AccountPtr pThis(new Account(IStackForInternal::queueCore(), delegate, conversationThreadDelegate, callDelegate));
        pThis->mThisWeak = pThis;

        String lockboxDomain(lockboxServiceDomain);
        IBootstrappedNetworkPtr lockboxNetwork = IBootstrappedNetwork::prepare(lockboxDomain);
        if (!lockboxNetwork) {
          ZS_LOG_ERROR(Detail, pThis->log("failed to prepare bootstrapped network for domain") + ", domain=" + lockboxDomain)
          return AccountPtr();
        }

        pThis->mGrantSession = IServiceNamespaceGrantSession::create(pThis, namespaceGrantOuterFrameURLUponReload, grantID);
        ZS_THROW_BAD_STATE_IF(!pThis->mGrantSession)

        pThis->mLockboxService = IServiceLockbox::createServiceLockboxFrom(lockboxNetwork);
        ZS_THROW_BAD_STATE_IF(!pThis->mLockboxService)

        pThis->mLockboxForceCreateNewAccount = forceCreateNewLockboxAccount;

        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      AccountPtr Account::relogin(
                                  IAccountDelegatePtr delegate,
                                  IConversationThreadDelegatePtr conversationThreadDelegate,
                                  ICallDelegatePtr callDelegate,
                                  const char *namespaceGrantOuterFrameURLUponReload,
                                  ElementPtr reloginInformation
                                  )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!namespaceGrantOuterFrameURLUponReload)
        ZS_THROW_INVALID_ARGUMENT_IF(!reloginInformation)

        AccountPtr pThis(new Account(IStackForInternal::queueCore(), delegate, conversationThreadDelegate, callDelegate));
        pThis->mThisWeak = pThis;

        String lockboxDomain;
        String accountID;
        String grantID;
        SecureByteBlockPtr lockboxKey;

        try {
          lockboxDomain = reloginInformation->findFirstChildElementChecked("lockboxDomain")->getTextDecoded();
          accountID = reloginInformation->findFirstChildElementChecked("accountID")->getTextDecoded();
          grantID = reloginInformation->findFirstChildElementChecked("grantID")->getTextDecoded();
          lockboxKey = services::IHelper::convertFromBase64(reloginInformation->findFirstChildElementChecked("lockboxKey")->getTextDecoded());
        } catch (CheckFailed &) {
          return AccountPtr();
        }

        IBootstrappedNetworkPtr lockboxNetwork = IBootstrappedNetwork::prepare(lockboxDomain);
        if (!lockboxNetwork) {
          ZS_LOG_ERROR(Detail, pThis->log("failed to prepare bootstrapped network for domain") + ", domain=" + lockboxDomain)
          return AccountPtr();
        }

        if (!lockboxKey) {
          ZS_LOG_ERROR(Detail, pThis->log("lockbox key specified in relogin information is not valid"))
          return AccountPtr();
        }

        pThis->mGrantSession = IServiceNamespaceGrantSession::create(pThis, namespaceGrantOuterFrameURLUponReload, grantID);
        ZS_THROW_BAD_STATE_IF(!pThis->mGrantSession)

        pThis->mLockboxService = IServiceLockbox::createServiceLockboxFrom(lockboxNetwork);
        ZS_THROW_BAD_STATE_IF(!pThis->mLockboxService)

        pThis->mLockboxForceCreateNewAccount = false;

        pThis->mLockboxSession = IServiceLockboxSession::relogin(pThis, pThis->mLockboxService, pThis->mGrantSession, accountID, *lockboxKey);
        pThis->init();

        if (!pThis->mLockboxSession) {
          ZS_LOG_ERROR(Detail, pThis->log("failed to create lockbox session from relogin information"))
          return AccountPtr();
        }
        return pThis;
      }

      //-----------------------------------------------------------------------
      IAccount::AccountStates Account::getState(
                                                WORD *outErrorCode,
                                                String *outErrorReason
                                                ) const
      {
        AutoRecursiveLock lock(getLock());

        if (outErrorCode) *outErrorCode = mLastErrorCode;
        if (outErrorReason) *outErrorReason = mLastErrorReason;

        ZS_LOG_DEBUG(log("getting account state") + getDebugValueString())

        return mCurrentState;
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::getReloginInformation() const
      {
        AutoRecursiveLock lock(getLock());

        if (!mLockboxService) {
          ZS_LOG_WARNING(Detail, log("missing lockbox domain information"))
          return ElementPtr();
        }

        String lockboxDomain = mLockboxService->getBootstrappedNetwork()->getDomain();
        if (lockboxDomain.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("missing lockbox domain information"))
          return ElementPtr();
        }

        if (!mGrantSession) {
          ZS_LOG_WARNING(Detail, log("missing namespace grant information"))
          return ElementPtr();
        }

        if (!mLockboxSession) {
          ZS_LOG_WARNING(Detail, log("missing lockbox session information"))
          return ElementPtr();
        }

        String accountID = mLockboxSession->getAccountID();
        if (accountID.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("missing account ID information"))
          return ElementPtr();
        }

        String grantID = mGrantSession->getGrantID();
        if (grantID.isEmpty()) {
          ZS_LOG_WARNING(Detail, log("missing grant ID information"))
          return ElementPtr();
        }

        SecureByteBlockPtr lockboxKey = mLockboxSession->getLockboxKey();

        if (!lockboxKey) {
          ZS_LOG_WARNING(Detail, log("missing lockbox key information"))
          return ElementPtr();
        }

        ElementPtr reloginEl = Element::create("relogin");

        reloginEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("lockboxDomain", lockboxDomain));
        reloginEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("accountID", accountID));
        reloginEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("grantID", grantID));
        reloginEl->adoptAsLastChild(IMessageHelper::createElementWithTextAndJSONEncode("lockboxKey", services::IHelper::convertToBase64(*lockboxKey)));

        return reloginEl;
      }

      //-----------------------------------------------------------------------
      String Account::getStableID() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mLockboxSession) return String();
        return mLockboxSession->getStableID();
      }

      //-----------------------------------------------------------------------
      String Account::getLocationID() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mStackAccount) return String();

        ILocationPtr self(ILocation::getForLocal(mStackAccount));
        if (!self) {
          ZS_LOG_WARNING(Detail, log("location ID is not available yet") + getDebugValueString())
          return String();
        }

        ZS_LOG_DEBUG(log("getting location") + ILocation::toDebugString(self))
        return self->getLocationID();
      }

      //-----------------------------------------------------------------------
      void Account::shutdown()
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("shutdown called") + getDebugValueString())
        cancel();
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::savePeerFilePrivate() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mLockboxSession) return ElementPtr();

        IPeerFilesPtr peerFiles = mLockboxSession->getPeerFiles();
        if (!peerFiles) {
          ZS_LOG_WARNING(Detail, log("peer files are not available") + getDebugValueString())
          return ElementPtr();
        }

        return peerFiles->saveToPrivatePeerElement();
      }

      //-----------------------------------------------------------------------
      SecureByteBlockPtr Account::getPeerFilePrivateSecret() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mLockboxSession) return SecureByteBlockPtr();

        IPeerFilesPtr peerFiles = mLockboxSession->getPeerFiles();
        if (!peerFiles) {
          ZS_LOG_WARNING(Detail, log("peer files are not available") + getDebugValueString())
          return SecureByteBlockPtr();
        }

        IPeerFilePrivatePtr peerFilePrivate = peerFiles->getPeerFilePrivate();
        return peerFilePrivate->getPassword();
      }

      //-----------------------------------------------------------------------
      IdentityListPtr Account::getAssociatedIdentities() const
      {
        AutoRecursiveLock lock(getLock());

        IdentityListPtr result(new IdentityList);

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("cannot get identities during shutdown") + getDebugValueString())
          return result;
        }

        ServiceIdentitySessionListPtr identities = mLockboxSession->getAssociatedIdentities();
        ZS_THROW_BAD_STATE_IF(!identities)

        for (ServiceIdentitySessionList::iterator iter = identities->begin(); iter != identities->end(); ++iter)
        {
          IServiceIdentitySessionPtr session = (*iter);
          IdentityMap::const_iterator found = mIdentities.find(session->getID());
          if (found != mIdentities.end()) {
            IdentityPtr identity = (*found).second;
            ZS_LOG_DEBUG(log("found existing identity") + IIdentity::toDebugString(identity))
            result->push_back(identity);
            continue;
          }

          IdentityPtr identity = IIdentityForAccount::createFromExistingSession(session);
          ZS_LOG_DEBUG(log("new identity found") + IIdentity::toDebugString(identity))
          mIdentities[identity->forAccount().getSession()->getID()] = identity;
          result->push_back(identity);
        }

        ZS_LOG_DEBUG(log("get associated identities complete") + ", total=" + string(result->size()))

        return result;
      }

      //-----------------------------------------------------------------------
      void Account::removeIdentities(const IdentityList &identitiesToRemove)
      {
        AutoRecursiveLock lock(getLock());

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("cannot associate identities during shutdown") + getDebugValueString())
          return;
        }

        if (!mLockboxSession) {
          ZS_LOG_WARNING(Detail, log("lockbox session has not yet been created"))
          return;
        }

        ServiceIdentitySessionList add;
        ServiceIdentitySessionList remove;

        for (IdentityList::const_iterator iter = identitiesToRemove.begin(); iter != identitiesToRemove.end(); ++iter)
        {
          IdentityPtr identity = Identity::convert(*iter);
          mIdentities[identity->forAccount().getSession()->getID()] = identity;
          remove.push_back(identity->forAccount().getSession());
        }

        mLockboxSession->associateIdentities(add, remove);
      }

      //-----------------------------------------------------------------------
      String Account::getInnerBrowserWindowFrameURL() const
      {
        AutoRecursiveLock lock(getLock());
        if (!mGrantSession) {
          ZS_LOG_WARNING(Detail, log("lockbox session has not yet been created"))
          return String();
        }
        return mGrantSession->getInnerBrowserWindowFrameURL();
      }

      //-----------------------------------------------------------------------
      void Account::notifyBrowserWindowVisible()
      {
        AutoRecursiveLock lock(getLock());
        if (!mGrantSession) {
          ZS_LOG_WARNING(Detail, log("lockbox session has not yet been created"))
          return;
        }
        mGrantSession->notifyBrowserWindowVisible();
      }

      //-----------------------------------------------------------------------
      void Account::notifyBrowserWindowClosed()
      {
        AutoRecursiveLock lock(getLock());
        if (!mGrantSession) {
          ZS_LOG_WARNING(Detail, log("lockbox session has not yet been created"))
          return;
        }
        mGrantSession->notifyBrowserWindowClosed();
      }

      //-----------------------------------------------------------------------
      ElementPtr Account::getNextMessageForInnerBrowerWindowFrame()
      {
        AutoRecursiveLock lock(getLock());
        if (!mGrantSession) {
          ZS_LOG_WARNING(Detail, log("lockbox session has not yet been created"))
          return ElementPtr();
        }
        DocumentPtr doc = mGrantSession->getNextMessageForInnerBrowerWindowFrame();
        if (!doc) {
          ZS_LOG_WARNING(Detail, log("lockbox has no message pending for inner browser window frame"))
          return ElementPtr();
        }
        ElementPtr root = doc->getFirstChildElement();
        ZS_THROW_BAD_STATE_IF(!root)
        root->orphan();
        return root;
      }

      //-----------------------------------------------------------------------
      void Account::handleMessageFromInnerBrowserWindowFrame(ElementPtr unparsedMessage)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!unparsedMessage)

        AutoRecursiveLock lock(getLock());
        if (!mGrantSession) {
          ZS_LOG_WARNING(Detail, log("lockbox session has not yet been created"))
          return;
        }
        DocumentPtr doc = Document::create();
        doc->adoptAsLastChild(unparsedMessage);
        mGrantSession->handleMessageFromInnerBrowserWindowFrame(doc);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForCall
      #pragma mark

      //-----------------------------------------------------------------------
      CallTransportPtr Account::getCallTransport() const
      {
        AutoRecursiveLock lock(mLock);
        return mCallTransport;
      }

      //-----------------------------------------------------------------------
      ICallDelegatePtr Account::getCallDelegate() const
      {
        AutoRecursiveLock lock(mLock);
        return mCallDelegate;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForContact
      #pragma mark

      //-----------------------------------------------------------------------
      ContactPtr Account::findContact(const char *peerURI) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!peerURI)

        AutoRecursiveLock lock(mLock);
        ContactMap::const_iterator found = mContacts.find(peerURI);
        if (found == mContacts.end()) {
          ZS_LOG_DEBUG(log("contact was not found for peer URI") + ", uri=" + peerURI)
          return ContactPtr();
        }
        const ContactPtr &contact = (*found).second;
        return contact;
      }

      //-----------------------------------------------------------------------
      void Account::notifyAboutContact(ContactPtr contact)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!contact)

        AutoRecursiveLock lock(mLock);
        String peerURI = contact->forAccount().getPeerURI();
        mContacts[peerURI] = contact;
      }

      //-----------------------------------------------------------------------
      void Account::hintAboutContactLocation(
                                             ContactPtr contact,
                                             const char *locationID
                                             )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!contact)
        ZS_THROW_INVALID_ARGUMENT_IF(!locationID)

        AutoRecursiveLock lock(mLock);

        ContactSubscriptionMap::iterator found = mContactSubscriptions.find(contact->forAccount().getPeerURI());
        if (found != mContactSubscriptions.end()) {
          ContactSubscriptionPtr contactSubscription = (*found).second;

          if ((contactSubscription->isShuttingDown()) ||
              (contactSubscription->isShutdown())) {
            // the contact subscription is dying, need to create a new one to replace the existing
            mContactSubscriptions.erase(found);
            found = mContactSubscriptions.end();
          }
        }

        if (found == mContactSubscriptions.end()) {
          // In this scenario we need to subscribe to this peer since we
          // do not have a connection established to this peer as of yet.
          ContactSubscriptionPtr contactSubscription = ContactSubscription::create(mThisWeak.lock(), contact);
          mContactSubscriptions[contact->forAccount().getPeerURI()] = contactSubscription;
        }

        // We need to hint about the contact location to the stack just in case
        // the stack does not know about this location.
        if (mStackAccount) {
          ILocationPtr location = ILocation::getForPeer(contact->forAccount().getPeer(), locationID);
          ZS_THROW_BAD_STATE_IF(!location)
          location->hintNowAvailable();
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForConversationThread
      #pragma mark

      //-----------------------------------------------------------------------
      ContactPtr Account::getSelfContact() const
      {
        AutoRecursiveLock lock(mLock);
        return mSelfContact;
      }

      //-----------------------------------------------------------------------
      ILocationPtr Account::getSelfLocation() const
      {
        AutoRecursiveLock lock(mLock);
        if (!mStackAccount) return ILocationPtr();

        return ILocation::getForLocal(mStackAccount);
      }

      //-----------------------------------------------------------------------
      stack::IAccountPtr Account::getStackAccount() const
      {
        AutoRecursiveLock lock(mLock);
        return mStackAccount;
      }

      //-----------------------------------------------------------------------
      IPublicationRepositoryPtr Account::getRepository() const
      {
        AutoRecursiveLock lock(mLock);
        if (!mStackAccount) return IPublicationRepositoryPtr();
        return IPublicationRepository::getFromAccount(mStackAccount);
      }

      //-----------------------------------------------------------------------
      IPeerFilesPtr Account::getPeerFiles() const
      {
        AutoRecursiveLock lock(mLock);
        if (!mLockboxSession) {
          ZS_LOG_WARNING(Detail, log("lockbox is not created yet thus peer files are not available yet"))
          return IPeerFilesPtr();
        }

        return mLockboxSession->getPeerFiles();
      }

      //-----------------------------------------------------------------------
      IConversationThreadDelegatePtr Account::getConversationThreadDelegate() const
      {
        AutoRecursiveLock lock(mLock);
        return mConversationThreadDelegate;
      }

      //-----------------------------------------------------------------------
      void Account::notifyConversationThreadCreated(ConversationThreadPtr thread)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!thread)
        AutoRecursiveLock lock(mLock);

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("cannot remember new thread or notify about it during shutdown") + getDebugValueString())
          return;
        }

        mConversationThreads[thread->forAccount().getThreadID()] = thread;

        try {
          mConversationThreadDelegate->onConversationThreadNew(thread);
        } catch (IConversationThreadDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("could not notify of new conversation thread - conversation thread delegate is gone"))
        }
      }

      //-----------------------------------------------------------------------
      ConversationThreadPtr Account::getConversationThreadByID(const char *threadID) const
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!threadID)
        AutoRecursiveLock lock(mLock);

        ConversationThreadMap::const_iterator found = mConversationThreads.find(threadID);
        if (found == mConversationThreads.end()) return ConversationThreadPtr();
        const ConversationThreadPtr &thread = (*found).second;
        return thread;
      }

      //-----------------------------------------------------------------------
      void Account::getConversationThreads(ConversationThreadList &outConversationThreads) const
      {
        AutoRecursiveLock lock(mLock);

        for (ConversationThreadMap::const_iterator iter = mConversationThreads.begin(); iter != mConversationThreads.end(); ++iter)
        {
          const ConversationThreadPtr &thread = (*iter).second;
          outConversationThreads.push_back(thread);
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForIdentity
      #pragma mark

      //-----------------------------------------------------------------------
      stack::IServiceNamespaceGrantSessionPtr Account::getNamespaceGrantSession() const
      {
        AutoRecursiveLock lock(mLock);
        return mGrantSession;
      }

      //-----------------------------------------------------------------------
      stack::IServiceLockboxSessionPtr Account::getLockboxSession() const
      {
        AutoRecursiveLock lock(mLock);
        return mLockboxSession;
      }

      //-----------------------------------------------------------------------
      void Account::associateIdentity(IdentityPtr identity)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!identity)

        ZS_LOG_DEBUG(log("associating identity to account/lockbox"))

        AutoRecursiveLock lock(mLock);

        mIdentities[identity->forAccount().getSession()->getID()] = identity;

        if (!mLockboxSession) {
          ZS_LOG_DEBUG(log("creating lockbox session"))
          mLockboxSession = IServiceLockboxSession::login(mThisWeak.lock(), mLockboxService, mGrantSession, identity->forAccount().getSession(), mLockboxForceCreateNewAccount);
        } else {
          ZS_LOG_DEBUG(log("associating to existing lockbox session"))
          ServiceIdentitySessionList add;
          ServiceIdentitySessionList remove;

          add.push_back(identity->forAccount().getSession());

          mLockboxSession->associateIdentities(add, remove);
        }

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IAccountForIdentityLookup
      #pragma mark

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => ICallTransportDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onCallTransportStateChanged(
                                                ICallTransportPtr inTransport,
                                                CallTransportStates state
                                                )
      {
        ZS_LOG_DEBUG(log("notified call transport state changed"))

        AutoRecursiveLock lock(mLock);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => stack::IAccountDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onAccountStateChanged(
                                          stack::IAccountPtr account,
                                          stack::IAccount::AccountStates state
                                          )
      {
        AutoRecursiveLock lock(mLock);
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IPeerSubscriptionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onPeerSubscriptionShutdown(IPeerSubscriptionPtr subscription)
      {
        AutoRecursiveLock lock(mLock);
        if (subscription != mPeerSubscription) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete subscription"))
          return;
        }

        mPeerSubscription.reset();
        step();
      }

      //-----------------------------------------------------------------------
      void Account::onPeerSubscriptionFindStateChanged(
                                                       IPeerSubscriptionPtr subscription,
                                                       IPeerPtr peer,
                                                       PeerFindStates state
                                                       )
      {
        // IGNORED
      }

      //-----------------------------------------------------------------------
      void Account::onPeerSubscriptionLocationConnectionStateChanged(
                                                                     IPeerSubscriptionPtr subscription,
                                                                     ILocationPtr location,
                                                                     LocationConnectionStates state
                                                                     )
      {
        AutoRecursiveLock lock(mLock);

        if (subscription != mPeerSubscription) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete subscription"))
          return;
        }

        IPeerPtr peer = location->getPeer();
        if (!peer) {
          ZS_LOG_WARNING(Detail, log("notified about location which is not a peer") + ILocation::toDebugString(location))
          return;
        }

        String peerURI = peer->getPeerURI();

        ZS_LOG_TRACE(log("notified peer location state changed") + ", state=" + ILocation::toString(state) + ILocation::toDebugString(location))

        // see if there is a local contact with this peer URI
        ContactMap::iterator foundContact = mContacts.find(peerURI);
        if (foundContact == mContacts.end()) {
          // did not find a contact with this peer URI - thus we need to create one
          IPeerFilePublicPtr peerFilePublic = peer->getPeerFilePublic();
          if (!peerFilePublic) {
            ZS_LOG_ERROR(Detail, log("no public peer file for location provided") + ILocation::toDebugString(location))
            return;
          }

          // create and remember this contact for the future
          ContactPtr contact = IContactForAccount::createFromPeer(mThisWeak.lock(), peer);

          // attempt find once more as contact might now be registered
          foundContact = mContacts.find(peerURI);
          ZS_THROW_BAD_STATE_IF(foundContact == mContacts.end())
        }

        ContactPtr contact = (*foundContact).second;

        ContactSubscriptionMap::iterator foundContactSubscription = mContactSubscriptions.find(peerURI);
        ContactSubscriptionPtr contactSubscription;
        if (foundContactSubscription == mContactSubscriptions.end()) {
          switch (state) {
            case ILocation::LocationConnectionState_Pending:
            case ILocation::LocationConnectionState_Disconnecting:
            case ILocation::LocationConnectionState_Disconnected:   {
              ZS_LOG_DEBUG(log("no need to create contact subscription when the connection is not ready") + ILocation::toDebugString(location))
              return;
            }
            case ILocation::LocationConnectionState_Connected: break;
          }

          ZS_LOG_DEBUG(log("creating a new contact subscription") + ILocation::toDebugString(location))
          contactSubscription = ContactSubscription::create(mThisWeak.lock(), contact, location);
          mContactSubscriptions[peerURI] = contactSubscription;
        } else {
          contactSubscription = (*foundContactSubscription).second;
        }

        ZS_LOG_DEBUG(log("notifying contact subscription about state") + ILocation::toDebugString(location))
        contactSubscription->notifyAboutLocationState(location, state);
      }

      //-----------------------------------------------------------------------
      void Account::onPeerSubscriptionMessageIncoming(
                                                      IPeerSubscriptionPtr subscription,
                                                      IMessageIncomingPtr message
                                                      )
      {
        // IGNORED
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IServiceLockboxSessionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onServiceLockboxSessionStateChanged(
                                                        IServiceLockboxSessionPtr session,
                                                        LockboxSessionStates state
                                                        )
      {
        AutoRecursiveLock lock(mLock);
        step();
      }

      //-----------------------------------------------------------------------
      void Account::onServiceLockboxSessionAssociatedIdentitiesChanged(IServiceLockboxSessionPtr session)
      {
        AutoRecursiveLock lock(mLock);

        if (session != mLockboxSession) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete peer contact session"))
          return;
        }

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("notified of association change during shutdown"))
          return;
        }

        ZS_THROW_BAD_STATE_IF(!mDelegate)

        try {
          mDelegate->onAccountAssociatedIdentitiesChanged(mThisWeak.lock());
        } catch(IAccountDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IServiceNamespaceGrantSessionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onServiceNamespaceGrantSessionStateChanged(
                                                               IServiceNamespaceGrantSessionPtr session,
                                                               GrantSessionStates state
                                                               )
      {
        AutoRecursiveLock lock(mLock);
        step();
      }

      //-----------------------------------------------------------------------
      void Account::onServiceNamespaceGrantSessionPendingMessageForInnerBrowserWindowFrame(IServiceNamespaceGrantSessionPtr session)
      {
        AutoRecursiveLock lock(mLock);

        if (session != mGrantSession) {
          ZS_LOG_WARNING(Detail, log("notified about obsolete namespace grant session"))
          return;
        }

        if ((isShuttingDown()) ||
            (isShutdown())) {
          ZS_LOG_WARNING(Detail, log("notified pending messages during shutdown"))
          return;
        }

        ZS_THROW_BAD_STATE_IF(!mDelegate)

        try {
          mDelegate->onAccountPendingMessageForInnerBrowserWindowFrame(mThisWeak.lock());
        } catch(IAccountDelegateProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => IWakeDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::onWake()
      {
        AutoRecursiveLock lock(mLock);
        ZS_LOG_DEBUG(log("on wake"))
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => friend Account::ContactSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::notifyContactSubscriptionShutdown(const String &peerURI)
      {
        AutoRecursiveLock lock(mLock);
        ContactSubscriptionMap::iterator found = mContactSubscriptions.find(peerURI);
        if (found == mContactSubscriptions.end()) return;

        mContactSubscriptions.erase(found);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => friend Account::LocationSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      ConversationThreadPtr Account::notifyPublicationUpdated(
                                                              ILocationPtr peerLocation,
                                                              IPublicationMetaDataPtr metaData,
                                                              const SplitMap &split
                                                              )
      {
        if (isShutdown()) {
          ZS_LOG_WARNING(Debug, log("received updated publication document after account was shutdown thus ignoring"))
          return ConversationThreadPtr();
        }

        String baseThreadID = services::IHelper::get(split, OPENPEER_CONVERSATION_THREAD_BASE_THREAD_ID_INDEX);
        String hostThreadID = services::IHelper::get(split, OPENPEER_CONVERSATION_THREAD_HOST_THREAD_ID_INDEX);
        if ((baseThreadID.size() < 1) ||
            (hostThreadID.size() < 1)) {
          ZS_LOG_WARNING(Debug, log("converation thread publication did not have a thread ID") + IPublicationMetaData::toDebugString(metaData))
          return ConversationThreadPtr();
        }

        ConversationThreadMap::iterator found = mConversationThreads.find(baseThreadID);
        if (found != mConversationThreads.end()) {
          ZS_LOG_DEBUG(log("notify publication updated for existing thread") + ", thread ID=" + baseThreadID + IPublicationMetaData::toDebugString(metaData))
          ConversationThreadPtr thread = (*found).second;
          thread->forAccount().notifyPublicationUpdated(peerLocation, metaData, split);
          return thread;
        }

        ZS_LOG_DEBUG(log("notify publication for new thread") + ", thread ID=" + baseThreadID + IPublicationMetaData::toDebugString(metaData))
        ConversationThreadPtr thread = IConversationThreadForAccount::create(mThisWeak.lock(), peerLocation, metaData, split);
        if (!thread) {
          ZS_LOG_WARNING(Debug, log("notify publication for new thread aborted"))
          return ConversationThreadPtr();
        }

        return thread;
      }

      //-----------------------------------------------------------------------
      void Account::notifyPublicationGone(
                                          ILocationPtr peerLocation,
                                          IPublicationMetaDataPtr metaData,
                                          const SplitMap &split
                                          )
      {
        String baseThreadID = services::IHelper::get(split, OPENPEER_CONVERSATION_THREAD_BASE_THREAD_ID_INDEX);
        String hostThreadID = services::IHelper::get(split, OPENPEER_CONVERSATION_THREAD_HOST_THREAD_ID_INDEX);
        if ((baseThreadID.size() < 1) ||
            (hostThreadID.size() < 1)) {
          ZS_LOG_WARNING(Debug, log("converation thread publication did not have a thread ID") + IPublicationMetaData::toDebugString(metaData))
          return;
        }

        ConversationThreadMap::iterator found = mConversationThreads.find(baseThreadID);
        if (found == mConversationThreads.end()) {
          ZS_LOG_WARNING(Debug, log("notify publication gone for thread that did not exist") + ", thread ID=" + baseThreadID + IPublicationMetaData::toDebugString(metaData))
          return;
        }

        ZS_LOG_DEBUG(log("notify publication gone for existing thread") + ", thread ID=" + baseThreadID + IPublicationMetaData::toDebugString(metaData))
        ConversationThreadPtr thread = (*found).second;
        thread->forAccount().notifyPublicationGone(peerLocation, metaData, split);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => internal
      #pragma mark

      //-----------------------------------------------------------------------
      String Account::log(const char *message) const
      {
        return String("core::Account [") + string(mID) + "] " + message;
      }

      //-----------------------------------------------------------------------
      String Account::getDebugValueString(bool includeCommaPrefix) const
      {
        AutoRecursiveLock lock(getLock());
        bool firstTime = !includeCommaPrefix;
        return Helper::getDebugValue("core account id", string(mID), firstTime) +
               Helper::getDebugValue("state", toString(mCurrentState), firstTime) +
               Helper::getDebugValue("error code", 0 != mLastErrorCode ? string(mLastErrorCode) : String(), firstTime) +
               Helper::getDebugValue("error reason", mLastErrorReason, firstTime) +
               Helper::getDebugValue("delegate", mDelegate ? String("true") : String(), firstTime) +
               Helper::getDebugValue("conversation thread delegate", mConversationThreadDelegate ? String("true") : String(), firstTime) +
               Helper::getDebugValue("call delegate", mCallDelegate ? String("true") : String(), firstTime) +
               stack::IAccount::toDebugString(mStackAccount) +
               stack::IServiceNamespaceGrantSession::toDebugString(mGrantSession) +
               stack::IServiceLockboxSession::toDebugString(mLockboxSession) +
               Helper::getDebugValue("force new lockbox account", mLockboxForceCreateNewAccount ? String("true") : String(), firstTime) +
               Helper::getDebugValue("identities", mIdentities.size() > 0 ? string(mIdentities.size()) : String(), firstTime) +
               stack::IPeerSubscription::toDebugString(mPeerSubscription) +
               IContact::toDebugString(mSelfContact) +
               Helper::getDebugValue("contacts", mContacts.size() > 0 ? string(mContacts.size()) : String(), firstTime) +
               Helper::getDebugValue("contact subscription", mContactSubscriptions.size() > 0 ? string(mContactSubscriptions.size()) : String(), firstTime) +
               Helper::getDebugValue("conversations", mConversationThreads.size() > 0 ? string(mConversationThreads.size()) : String(), firstTime) +
               Helper::getDebugValue("call transport", mCallTransport ? String("true") : String(), firstTime) +
               Helper::getDebugValue("subscribers permission document", mSubscribersPermissionDocument ? String("true") : String(), firstTime);
      }

      //-----------------------------------------------------------------------
      void Account::cancel()
      {
        AutoRecursiveLock lock(mLock);  // just in case

        ZS_LOG_DEBUG(log("cancel called") + getDebugValueString())

        if (isShutdown()) return;
        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        setState(AccountState_ShuttingDown);

        if (mCallTransport) {
          ZS_LOG_DEBUG(log("shutting down call transport"))
          mCallTransport->forAccount().shutdown();
        }

        if (mStackAccount) {
          ZS_LOG_DEBUG(log("shutting down stack account"))
          mStackAccount->shutdown();
        }

        if (mPeerSubscription) {
          mPeerSubscription->cancel();
          mPeerSubscription.reset();
        }

        if (mGracefulShutdownReference) {
          if (mStackAccount) {
            if (stack::IAccount::AccountState_Shutdown != mStackAccount->getState()) {
              ZS_LOG_DEBUG(log("waiting for stack account to shutdown"))
              return;
            }
          }

          if (mCallTransport) {
            if (ICallTransport::CallTransportState_Shutdown != mCallTransport->forAccount().getState()) {
              ZS_LOG_DEBUG(log("waiting for call transport to shutdown"))
              return;
            }
          }
        }

        setState(AccountState_Shutdown);

        if (mGrantSession) {
          mGrantSession->cancel();    // do not reset
        }

        if (mLockboxSession) {
          mLockboxSession->cancel();  // do not reset
        }

        mGracefulShutdownReference.reset();

        mDelegate.reset();
        mConversationThreadDelegate.reset();
        mCallDelegate.reset();

        mStackAccount.reset();
        mCallTransport.reset();

        ZS_LOG_DEBUG(log("shutdown complete"))
      }

      //-----------------------------------------------------------------------
      void Account::step()
      {
        if ((isShutdown()) ||
            (isShuttingDown())) {
          ZS_LOG_DEBUG(log("step forwarding to cancel"))
          cancel();
          return;
        }

        ZS_LOG_DEBUG(log("step") + getDebugValueString())

        if (!stepLoginIdentityAssociated()) return;
        if (!stepLockboxShutdownCheck()) return;
        if (!stepGrantSession()) return;
        if (!stepLockboxSession()) return;
        if (!stepStackAccount()) return;
        if (!stepSelfContact()) return;
        if (!stepCallTransportSetup()) return;
        if (!stepSubscribersPermissionDocument()) return;
        if (!stepPeerSubscription()) return;
        if (!stepCallTransportFinalize()) return;

        setState(AccountState_Ready);

        ZS_LOG_DEBUG(log("step complete") + getDebugValueString())
      }

      //-----------------------------------------------------------------------
      bool Account::stepLoginIdentityAssociated()
      {
        if (mLockboxSession) {
          ZS_LOG_DEBUG(log("lockbox is already created thus login identity associate is not needed"))
          return true;
        }

        ZS_LOG_DEBUG(log("waiting for account to be associated to an identity"))

        setState(AccountState_WaitingForAssociationToIdentity);
        return false;
      }

      //-----------------------------------------------------------------------
      bool Account::stepLockboxShutdownCheck()
      {
        WORD errorCode = 0;
        String reason;

        IServiceLockboxSession::SessionStates state = mLockboxSession->getState(&errorCode, &reason);
        if (IServiceLockboxSession::SessionState_Shutdown == state) {
          ZS_LOG_ERROR(Detail, log("lockbox session shutdown"))
          setError(errorCode, reason);
          cancel();
          return false;
        }

        ZS_LOG_TRACE(log("lockbox is not shutdown thus allowing to continue"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepGrantSession()
      {
        WORD errorCode = 0;
        String reason;

        IServiceNamespaceGrantSession::SessionStates state = mGrantSession->getState(&errorCode, &reason);

        switch (state) {
          case IServiceNamespaceGrantSession::SessionState_Pending:
          {
            ZS_LOG_TRACE(log("namespace grant session is pending"))
            setState(AccountState_Pending);
            return false;
          }
          case IServiceNamespaceGrantSession::SessionState_WaitingForBrowserWindowToBeLoaded:
          {
            ZS_LOG_TRACE(log("namespace grant is waiting for the browser window to be loaded"))
            setState(AccountState_WaitingForBrowserWindowToBeLoaded);
            return false;
          }
          case IServiceNamespaceGrantSession::SessionState_WaitingForBrowserWindowToBeMadeVisible:
          {
            ZS_LOG_TRACE(log("namespace grant is waiting for browser window to be made visible"))
            setState(AccountState_WaitingForBrowserWindowToBeMadeVisible);
            return false;
          }
          case IServiceNamespaceGrantSession::SessionState_WaitingForBrowserWindowToClose:
          {
            ZS_LOG_TRACE(log("namespace grant is waiting for browser window to close"))
            setState(AccountState_WaitingForBrowserWindowToClose);
            return false;
          }
          case IServiceNamespaceGrantSession::SessionState_Ready: {
            ZS_LOG_TRACE(log("namespace grant is ready"))
            return true;
          }
          case IServiceNamespaceGrantSession::SessionState_Shutdown:  {
            ZS_LOG_ERROR(Detail, log("namespace grant is session shutdown"))
            setError(errorCode, reason);
            cancel();
            return false;
          }
        }

        ZS_LOG_DEBUG(log("waiting for lockbox session to be ready"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool Account::stepLockboxSession()
      {
        WORD errorCode = 0;
        String reason;

        IServiceLockboxSession::SessionStates state = mLockboxSession->getState(&errorCode, &reason);

        switch (state) {
          case IServiceLockboxSession::SessionState_Pending:
          {
            ZS_LOG_DEBUG(log("lockbox is pending"))
            setState(AccountState_Pending);
            return false;
          }
          case IServiceLockboxSession::SessionState_PendingPeerFilesGeneration:
          {
            ZS_LOG_DEBUG(log("lockbox is pending and generating peer files"))
            setState(AccountState_PendingPeerFilesGeneration);
            return false;
          }
          case IServiceLockboxSession::SessionState_Ready: {
            ZS_LOG_DEBUG(log("lockbox session is ready"))
            return true;
          }
          case IServiceLockboxSession::SessionState_Shutdown:  {
            ZS_LOG_ERROR(Detail, log("lockbox session shutdown"))
            setError(errorCode, reason);
            cancel();
            return false;
          }
        }

        ZS_LOG_DEBUG(log("waiting for lockbox session to be ready"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool Account::stepStackAccount()
      {
        ZS_THROW_BAD_STATE_IF(!mLockboxSession)

        if (!mStackAccount) {
          mStackAccount = stack::IAccount::create(mThisWeak.lock(), mLockboxSession);
        }

        WORD errorCode = 0;
        String reason;

        stack::IAccount::AccountStates state = mStackAccount->getState(&errorCode, &reason);

        if (stack::IAccount::AccountState_Ready == state) {
          ZS_LOG_DEBUG(log("step peer contact completed"))
          return true;
        }

        if ((stack::IAccount::AccountState_ShuttingDown == state) ||
            (stack::IAccount::AccountState_Shutdown == state)) {
          ZS_LOG_ERROR(Detail, log("peer contact session shutdown"))
          setError(errorCode, reason);
          cancel();
          return false;
        }

        ZS_LOG_DEBUG(log("waiting for stack account session to be ready"))
        return false;
      }

      //-----------------------------------------------------------------------
      bool Account::stepSelfContact()
      {
        if (mSelfContact) {
          ZS_LOG_DEBUG(log("contact self ready"))
          return true;
        }

        ILocationPtr selfLocation = ILocation::getForLocal(mStackAccount);
        if (!selfLocation) {
          ZS_LOG_ERROR(Detail, log("could not obtain self location"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Could not obtain location for self");
          cancel();
          return false;
        }

        mSelfContact = IContactForAccount::createFromPeer(mThisWeak.lock(), selfLocation->getPeer());
        ZS_THROW_BAD_STATE_IF(!mSelfContact)
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepCallTransportSetup()
      {
        if (mCallTransport) {
          ICallTransportForAccount::CallTransportStates state = mCallTransport->forAccount().getState();
          if ((ICallTransport::CallTransportState_ShuttingDown == state) ||
              (ICallTransport::CallTransportState_Shutdown == state)){
            ZS_LOG_ERROR(Detail, log("premature shutdown of transport object (something is wrong)"))
            setError(IHTTP::HTTPStatusCode_InternalServerError, "Call transport shutdown unexpectedly");
            cancel();
            return false;
          }

          ZS_LOG_DEBUG(log("call transport ready"))
          return true;
        }

        String turnServer;
        String turnUsername;
        String turnPassword;
        String stunServer;

        mStackAccount->getNATServers(turnServer, turnUsername, turnPassword, stunServer);
        mCallTransport = ICallTransportForAccount::create(mThisWeak.lock(), turnServer, turnUsername, turnPassword, stunServer);

        if (!mCallTransport) {
          ZS_LOG_ERROR(Detail, log("failed to create call transport object thus shutting down"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Call transport failed to create");
          cancel();
          return false;
        }

        ZS_LOG_DEBUG(log("call transport is setup"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepSubscribersPermissionDocument()
      {
        if (mSubscribersPermissionDocument) {
          ZS_LOG_DEBUG(log("permission document ready"))
          return true;
        }

        IPublicationRepositoryPtr repository = getRepository();
        if (!repository) {
          ZS_LOG_ERROR(Detail, log("repository on stack account is not valid thus account must shutdown"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Repository object is missing");
          cancel();
          return false;
        }

        IPublication::RelationshipList relationships;
        relationships.push_back(mSelfContact->forAccount().getPeerURI());

        ILocationPtr selfLocation = ILocation::getForLocal(mStackAccount);

        stack::IPublicationMetaData::PublishToRelationshipsMap empty;
        mSubscribersPermissionDocument = stack::IPublication::create(selfLocation, "/threads/1.0/subscribers/permissions", "text/x-json-openpeer-permissions", relationships, empty, selfLocation);
        if (!mSubscribersPermissionDocument) {
          ZS_LOG_ERROR(Detail, log("unable to create subscription permission document thus shutting down"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Failed to create subscribers document");
          cancel();
          return false;
        }

        IPublicationPublisherPtr publisher = repository->publish(IPublicationPublisherDelegateProxy::createNoop(getAssociatedMessageQueue()), mSubscribersPermissionDocument);
        if (!publisher->isComplete()) {
          ZS_LOG_ERROR(Detail, log("unable to publish local subscription permission document which should have happened instantly"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Failed to publish document to self");
          cancel();
          return false;
        }
        ZS_LOG_DEBUG(log("subscribers permission document created"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepPeerSubscription()
      {
        if (mPeerSubscription) {
          ZS_LOG_DEBUG(log("peer subscription ready"))
          return true;
        }

        mPeerSubscription = IPeerSubscription::subscribeAll(mStackAccount, mThisWeak.lock());

        if (!mPeerSubscription) {
          ZS_LOG_ERROR(Detail, log("unable to create a subscription to all connections"))
          setError(IHTTP::HTTPStatusCode_InternalServerError, "Failed to create peer subscription");
          cancel();
          return false;
        }

        ZS_LOG_DEBUG(log("peer subscription created"))
        return true;
      }

      //-----------------------------------------------------------------------
      bool Account::stepCallTransportFinalize()
      {
        if (ICallTransport::CallTransportState_Ready == mCallTransport->forAccount().getState()) {
          ZS_LOG_DEBUG(log("call transport is finalized"))
          return true;
        }
        ZS_LOG_DEBUG(log("waiting on call transport to be ready"))
        return false;
      }

      //-----------------------------------------------------------------------
      void Account::setState(IAccount::AccountStates state)
      {
        if (mCurrentState == state) return;

        ZS_LOG_BASIC(log("state changed") + ", new state=" + toString(state) + getDebugValueString())
        mCurrentState = state;

        AccountPtr pThis = mThisWeak.lock();

        if (pThis) {
          try {
            mDelegate->onAccountStateChanged(mThisWeak.lock(), state);
          } catch (IAccountDelegateProxy::Exceptions::DelegateGone &) {
            ZS_LOG_WARNING(Detail, log("delegate gone"))
          }
        }
      }

      //-----------------------------------------------------------------------
      void Account::setError(
                             WORD errorCode,
                             const char *inReason
                             )
      {
        if (0 == errorCode) {
          ZS_LOG_DEBUG(log("no error specified"))
          return;
        }

        String reason(inReason ? String(inReason) : String());
        if (reason.isEmpty()) {
          reason = IHTTP::toString(IHTTP::toStatusCode(errorCode));
        }

        if (0 != mLastErrorCode) {
          ZS_LOG_WARNING(Detail, log("error was already set (thus ignoring)") + ", new error=" + string(errorCode) + ", new reason=" + reason + getDebugValueString())
          return;
        }

        mLastErrorCode = errorCode;
        mLastErrorReason = reason;
        ZS_LOG_ERROR(Detail, log("account error") + getDebugValueString())
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => ContactSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      const char *Account::ContactSubscription::toString(Account::ContactSubscription::ContactSubscriptionStates state)
      {
        switch (state) {
          case ContactSubscriptionState_Pending:      return "Pending";
          case ContactSubscriptionState_Ready:        return "Ready";
          case ContactSubscriptionState_ShuttingDown: return "Shutting down";
          case ContactSubscriptionState_Shutdown:     return "Shutdown";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      Account::ContactSubscription::ContactSubscription(
                                                        AccountPtr outer,
                                                        ContactPtr contact
                                                        ) :
        MessageQueueAssociator(outer->getAssociatedMessageQueue()),
        mID(zsLib::createPUID()),
        mOuter(outer),
        mContact(contact),
        mCurrentState(ContactSubscriptionState_Pending)
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!outer)
        ZS_THROW_INVALID_ARGUMENT_IF(!contact)
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::init(ILocationPtr peerLocation)
      {
        if (!peerLocation) {
          ZS_LOG_DEBUG(log("creating a contact subscription to a hinted location") + IContact::toDebugString(mContact))

          // If there isn't a peer location then this contact subscription came
          // into being because this contact was hinted that it wants to connect
          // with this user. As such we will need to open a peer subscription
          // to the contact to cause locations to be found/opened. If there
          // are active conversation threads then the conversation threads will
          // open their own peer subscriptions and thus this peer subscription
          // can be shutdown after a reasonable amount of time has passed to
          // try to connect to the peer.
          AccountPtr outer = mOuter.lock();
          ZS_THROW_BAD_STATE_IF(!outer)

          stack::IAccountPtr stackAccount = outer->getStackAccount();
          if (!stackAccount) {
            ZS_LOG_WARNING(Detail, log("stack account is not available thus unable to create contact subscription"))
            goto step;
          }

          IPeerFilePublicPtr peerFilePublic = mContact->forAccount().getPeerFilePublic();
          if (!peerFilePublic) {
            ZS_LOG_WARNING(Detail, log("public peer file for contact is not available"))
            goto step;
          }

          IPeerPtr peer = mContact->forAccount().getPeer();

          mPeerSubscription = IPeerSubscription::subscribe(peer, mThisWeak.lock());
          ZS_THROW_BAD_STATE_IF(!mPeerSubscription)

          mPeerSubscriptionAutoCloseTimer = Timer::create(mThisWeak.lock(), Seconds(OPENPEER_PEER_SUBSCRIPTION_AUTO_CLOSE_TIMEOUT_IN_SECONDS), false);
        } else {
          ZS_LOG_DEBUG(log("creating location subscription to location") + ILocation::toDebugString(peerLocation))
          mLocations[peerLocation->getLocationID()] = LocationSubscription::create(mThisWeak.lock(), peerLocation);
        }

      step:
        step();
      }

      //-----------------------------------------------------------------------
      Account::ContactSubscription::~ContactSubscription()
      {
        ZS_LOG_DEBUG(log("destructor called"))
        mThisWeak.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      String Account::ContactSubscription::toDebugString(ContactSubscriptionPtr contactSubscription, bool includeCommaPrefix)
      {
        if (!contactSubscription) return includeCommaPrefix ? String(", contact subscription=(null)") : String("contact subscription=(null)");
        return contactSubscription->getDebugValueString(includeCommaPrefix);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::ContactSubscription => friend Account
      #pragma mark

      //-----------------------------------------------------------------------
      Account::ContactSubscriptionPtr Account::ContactSubscription::create(
                                                                           AccountPtr outer,
                                                                           ContactPtr contact,
                                                                           ILocationPtr peerLocation
                                                                           )
      {
        ContactSubscriptionPtr pThis(new ContactSubscription(outer, contact));
        pThis->mThisWeak = pThis;
        pThis->init(peerLocation);
        return pThis;
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::notifyAboutLocationState(
                                                                  ILocationPtr location,
                                                                  ILocation::LocationConnectionStates state
                                                                  )
      {
        LocationSubscriptionMap::iterator found = mLocations.find(location->getLocationID());

        LocationSubscriptionPtr locationSubscription;
        if (found != mLocations.end()) {
          locationSubscription = (*found).second;
        }

        ZS_LOG_DEBUG(log("notifying about location state") + ", state=" + ILocation::toString(state) + ", found=" + (found != mLocations.end() ? "true" : "false") + ILocation::toDebugString(location))

        switch (state) {
          case ILocation::LocationConnectionState_Pending: {
            if (found == mLocations.end()) {
              ZS_LOG_DEBUG(log("pending state where location is not found thus do nothing"))
              return;  // only do something when its actually connected
            }

            ZS_LOG_DEBUG(log("pending state where location is found thus cancelling existing location"))

            // we must have had an old subscription laying around, kill it in favour of a new one that will come later...
            locationSubscription->cancel();
            break;
          }
          case ILocation::LocationConnectionState_Connected: {
            if (found != mLocations.end()) {
              // make sure the location that already exists isn't in the middle of a shutdown...
              if ((locationSubscription->isShuttingDown()) || (locationSubscription->isShutdown())) {
                ZS_LOG_WARNING(Debug, log("connected state where location subscription was shutting down thus forgetting location subscription early"))

                // forget about this location early since it must shutdown anyway...
                mLocations.erase(found);
                found = mLocations.end();
              }
            }

            if (found != mLocations.end()) {
              ZS_LOG_DEBUG(log("connected state where location subscription is pending or ready"))
              return;  // nothing to do since location already exists
            }

            ZS_LOG_DEBUG(log("creating location subscription for connected location"))

            // we have a new location, remember it...
            locationSubscription = LocationSubscription::create(mThisWeak.lock(), location);
            mLocations[location->getLocationID()] = locationSubscription;
            break;
          }
          case ILocation::LocationConnectionState_Disconnecting:
          case ILocation::LocationConnectionState_Disconnected:  {
            if (found == mLocations.end()) {
              ZS_LOG_DEBUG(log("ignoring disconnecting/disconnected state where there is no location subscription"))
              return;  // nothing to do as we don't have location anyway...
            }

            ZS_LOG_DEBUG(log("cancelling location subscription for disconnecting/disconnected location"))
            locationSubscription->cancel();
            break;
          }
        }
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::ContactSubscription => IContactSubscriptionAsyncDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::onWake()
      {
        AutoRecursiveLock lock(getLock());
        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::ContactSubscription => IPeerSubscriptionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::onPeerSubscriptionShutdown(IPeerSubscriptionPtr subscription)
      {
        AutoRecursiveLock lock(getLock());
        if (mPeerSubscription != subscription) {
          ZS_LOG_DEBUG(log("ignoring peer subscription shutdown for obslete subscription"))
          return;
        }

        ZS_LOG_DEBUG(log("peer subscription shutdown"))

        mPeerSubscription.reset();
        step();
      }

      void Account::ContactSubscription::onPeerSubscriptionFindStateChanged(
                                                                            IPeerSubscriptionPtr subscription,
                                                                            IPeerPtr peer,
                                                                            PeerFindStates state
                                                                            )
      {
        AutoRecursiveLock lock(getLock());
        ZS_LOG_DEBUG(log("peer subscription find state changed") + ", state=" + IPeer::toString(state) + IPeer::toDebugString(peer))
        step();
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::onPeerSubscriptionLocationConnectionStateChanged(
                                                                                          IPeerSubscriptionPtr subscription,
                                                                                          ILocationPtr location,
                                                                                          LocationConnectionStates state
                                                                                          )
      {
        AutoRecursiveLock lock(getLock());
        if (mPeerSubscription != subscription) {
          ZS_LOG_DEBUG(log("ignoring peer subscription shutdown for obslete subscription"))
          return;
        }

        ZS_LOG_DEBUG(log("peer subscription location state changed") + ", state=" + ILocation::toString(state) + ILocation::toDebugString(location))
        step();
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::onPeerSubscriptionMessageIncoming(
                                                                           IPeerSubscriptionPtr subscription,
                                                                           IMessageIncomingPtr incomingMessage
                                                                           )
      {
        //IGNORED
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::ContactSubscription => ITimerDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::onTimer(TimerPtr timer)
      {
        AutoRecursiveLock lock(getLock());
        if (timer != mPeerSubscriptionAutoCloseTimer) return;

        ZS_LOG_DEBUG(log("timer fired") + IPeerSubscription::toDebugString(mPeerSubscription))

        if (mPeerSubscription) {
          mPeerSubscription->cancel();
          mPeerSubscription.reset();
        }

        mPeerSubscriptionAutoCloseTimer->cancel();
        mPeerSubscriptionAutoCloseTimer.reset();

        step();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::ContactSubscription => friend LocationSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      AccountPtr Account::ContactSubscription::getOuter() const
      {
        AutoRecursiveLock lock(getLock());
        return mOuter.lock();
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::notifyLocationShutdown(const String &locationID)
      {
        AutoRecursiveLock lock(getLock());

        LocationSubscriptionMap::iterator found = mLocations.find(locationID);
        if (found == mLocations.end()) {
          ZS_LOG_DEBUG(log("location subscription not found in connection subscription list") + ", location ID=" + locationID)
          return;
        }

        ZS_LOG_DEBUG(log("erasing location subscription") + ", location ID=" + locationID)
        mLocations.erase(found);

        IWakeDelegateProxy::create(mThisWeak.lock())->onWake();
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::ContactSubscription => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      RecursiveLock &Account::ContactSubscription::getLock() const
      {
        AccountPtr outer = mOuter.lock();
        if (!outer) return mBogusLock;
        return outer->getLock();
      }

      //-----------------------------------------------------------------------
      String Account::ContactSubscription::log(const char *message) const
      {
        return String("openpeer::Account::ContactSubscription [") + string(mID) + "] " + message + ", peer URI=" + mContact->forAccount().getPeerURI();
      }

      //-----------------------------------------------------------------------
      String Account::ContactSubscription::getDebugValueString(bool includeCommaPrefix) const
      {
        AutoRecursiveLock lock(getLock());
        bool firstTime = !includeCommaPrefix;
        return Helper::getDebugValue("contact subscription id", string(mID), firstTime) +
               Helper::getDebugValue("state", toString(mCurrentState), firstTime) +
               IContact::toDebugString(mContact) +
               IPeerSubscription::toDebugString(mPeerSubscription) +
               Helper::getDebugValue("timer", mPeerSubscriptionAutoCloseTimer ? String("true") : String(), firstTime) +
               Helper::getDebugValue("locations", mLocations.size() > 0 ? string(mLocations.size()) : String(), firstTime);
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::cancel()
      {
        if (isShutdown()) return;

        setState(ContactSubscriptionState_ShuttingDown);

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        bool locationsShutdown = true;

        // clear out locations
        {
          for (LocationSubscriptionMap::iterator locIter = mLocations.begin(); locIter != mLocations.end(); )
          {
            LocationSubscriptionMap::iterator current = locIter;
            ++locIter;

            LocationSubscriptionPtr &location = (*current).second;

            location->cancel();
            if (!location->isShutdown()) locationsShutdown = false;
          }

          mLocations.clear();
        }

        if (mPeerSubscription) {
          mPeerSubscription->cancel();
        }

        if (mGracefulShutdownReference) {
          if (mPeerSubscription) {
            if (!mPeerSubscription->isShutdown()) {
              ZS_LOG_DEBUG(log("waiting for peer subscription to shutdown"))
              return;
            }
          }

          if (!locationsShutdown) {
            ZS_LOG_DEBUG(log("waiting for location to shutdown"))
            return;
          }
        }

        setState(ContactSubscriptionState_Shutdown);

        mGracefulShutdownReference.reset();

        if (mPeerSubscriptionAutoCloseTimer) {
          mPeerSubscriptionAutoCloseTimer->cancel();
          mPeerSubscriptionAutoCloseTimer.reset();
        }

        mLocations.clear();

        if (mPeerSubscription) {
          mPeerSubscription->cancel();
          mPeerSubscription.reset();
        }

        AccountPtr outer = mOuter.lock();
        if (outer) {
          outer->notifyContactSubscriptionShutdown(mContact->forAccount().getPeerURI());
        }
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::step()
      {
        if ((isShuttingDown()) ||
            (isShutdown())) {
          cancel();
          return;
        }

        setState(ContactSubscriptionState_Ready);

        if (!mPeerSubscriptionAutoCloseTimer) {
          if (mLocations.size() < 1) {
            // there are no more locations... we should shut outselves down...
            cancel();
          }
        }
      }

      //-----------------------------------------------------------------------
      void Account::ContactSubscription::setState(ContactSubscriptionStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(log("state changed") + ", old state=" + toString(mCurrentState) + ", new state=" + toString(state))
        mCurrentState = state;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account => LocationSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      const char *Account::LocationSubscription::toString(Account::LocationSubscription::LocationSubscriptionStates state)
      {
        switch (state) {
          case LocationSubscriptionState_Pending:      return "Pending";
          case LocationSubscriptionState_Ready:        return "Ready";
          case LocationSubscriptionState_ShuttingDown: return "Shutting down";
          case LocationSubscriptionState_Shutdown:     return "Shutdown";
        }
        return "UNDEFINED";
      }

      //-----------------------------------------------------------------------
      Account::LocationSubscription::LocationSubscription(
                                                          ContactSubscriptionPtr outer,
                                                          ILocationPtr peerLocation
                                                          ) :
        MessageQueueAssociator(outer->getAssociatedMessageQueue()),
        mID(zsLib::createPUID()),
        mOuter(outer),
        mPeerLocation(peerLocation),
        mCurrentState(LocationSubscriptionState_Pending)
      {
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::init()
      {
        step();
      }

      //-----------------------------------------------------------------------
      Account::LocationSubscription::~LocationSubscription()
      {
        ZS_LOG_DEBUG(log("destructor called"))
        mThisWeak.reset();
        cancel();
      }

      //-----------------------------------------------------------------------
      String Account::LocationSubscription::toDebugString(LocationSubscriptionPtr subscription, bool includeCommaPrefix)
      {
        if (!subscription) return includeCommaPrefix ? ", location subscription=(null)" : "location subscription=(null)";
        return subscription->getDebugValueString(includeCommaPrefix);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::LocationSubscription => friend ContactSubscription
      #pragma mark

      //-----------------------------------------------------------------------
      Account::LocationSubscriptionPtr Account::LocationSubscription::create(
                                                                             ContactSubscriptionPtr outer,
                                                                             ILocationPtr peerLocation
                                                                             )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!peerLocation)
        LocationSubscriptionPtr pThis(new LocationSubscription(outer, peerLocation));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::LocationSubscription => IPublicationSubscriptionDelegate
      #pragma mark

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::onPublicationSubscriptionStateChanged(
                                                                                IPublicationSubscriptionPtr subscription,
                                                                                PublicationSubscriptionStates state
                                                                                )
      {
        AutoRecursiveLock lock(getLock());
        if (subscription != mPublicationSubscription) {
          ZS_LOG_DEBUG(log("ignoring publication subscription state change for obsolete subscription"))
          return;
        }

        ZS_LOG_DEBUG(log("publication subscription state change") + ", state=" + IPublicationSubscription::toString(state) + IPublicationSubscription::toDebugString(subscription))

        if ((stack::IPublicationSubscription::PublicationSubscriptionState_ShuttingDown == mPublicationSubscription->getState()) ||
            (stack::IPublicationSubscription::PublicationSubscriptionState_ShuttingDown == mPublicationSubscription->getState())) {
          ZS_LOG_WARNING(Detail, log("failed to create a subscription to the peer"))
          mPublicationSubscription.reset();
          cancel();
          return;
        }

        step();
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::onPublicationSubscriptionPublicationUpdated(
                                                                                      IPublicationSubscriptionPtr subscription,
                                                                                      IPublicationMetaDataPtr metaData
                                                                                      )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!metaData)

        AutoRecursiveLock lock(getLock());
        if (subscription != mPublicationSubscription) {
          ZS_LOG_DEBUG(log("ignoring publication notification on obsolete publication subscription"))
          return;
        }

        String name = metaData->getName();

        SplitMap result;
        services::IHelper::split(name, result);

        if (result.size() < 6) {
          ZS_LOG_WARNING(Debug, log("subscription path is too short") + IPublicationMetaData::toDebugString(metaData))
          return;
        }

        ContactSubscriptionPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Debug, log("unable to locate contact subscription"))
          return;
        }

        AccountPtr account = outer->getOuter();
        if (!account) {
          ZS_LOG_WARNING(Debug, log("unable to locate account"))
          return;
        }

        ConversationThreadPtr thread = account->notifyPublicationUpdated(mPeerLocation, metaData, result);
        if (!thread) {
          ZS_LOG_WARNING(Debug, log("publication did not result in a conversation thread"))
          return;
        }

        String threadID = thread->forAccount().getThreadID();
        ConversationThreadMap::iterator found = mConversationThreads.find(threadID);
        if (found != mConversationThreads.end()) {
          ZS_LOG_DEBUG(log("already know about this conversation thread (thus nothing more to do)"))
          return;  // already know about this conversation thread
        }

        ZS_LOG_DEBUG(log("remembering converation thread for the future"))

        // remember this conversation thread is linked to this peer location
        mConversationThreads[threadID] = thread;
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::onPublicationSubscriptionPublicationGone(
                                                                                   IPublicationSubscriptionPtr subscription,
                                                                                   IPublicationMetaDataPtr metaData
                                                                                   )
      {
        ZS_THROW_INVALID_ARGUMENT_IF(!metaData)

        AutoRecursiveLock lock(getLock());
        if (subscription != mPublicationSubscription) {
          ZS_LOG_DEBUG(log("ignoring publication notification on obsolete publication subscription"))
          return;
        }

        String name = metaData->getName();

        SplitMap result;
        services::IHelper::split(name, result);

        if (result.size() < 6) {
          ZS_LOG_WARNING(Debug, log("subscription path is too short") + ", path=" + name)
          return;
        }

        ContactSubscriptionPtr outer = mOuter.lock();
        if (!outer) {
          ZS_LOG_WARNING(Debug, log("unable to locate contact subscription"))
          return;
        }

        AccountPtr account = outer->getOuter();
        if (!account) {
          ZS_LOG_WARNING(Debug, log("unable to locate account"))
          return;
        }

        account->notifyPublicationGone(mPeerLocation, metaData, result);
      }

      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      #pragma mark
      #pragma mark Account::LocationSubscription => (internal)
      #pragma mark

      //-----------------------------------------------------------------------
      RecursiveLock &Account::LocationSubscription::getLock() const
      {
        ContactSubscriptionPtr outer = mOuter.lock();
        if (!outer) return mBogusLock;
        return outer->getLock();
      }

      //-----------------------------------------------------------------------
      String Account::LocationSubscription::log(const char *message) const
      {
        return String("openpeer::Account::LocationSubscription [") + string(mID) + "] " + message + ", peer URI=" + getPeerURI() + ", location ID=" + getLocationID();
      }

      //-----------------------------------------------------------------------
      String Account::LocationSubscription::getDebugValueString(bool includeCommaPrefix) const
      {
        AutoRecursiveLock lock(getLock());
        bool firstTime = !includeCommaPrefix;
        return Helper::getDebugValue("location subscription id", string(mID), firstTime) +
               Helper::getDebugValue("state", toString(mCurrentState), firstTime) +
               ILocation::toDebugString(mPeerLocation) +
               IPublicationSubscription::toDebugString(mPublicationSubscription) +
               Helper::getDebugValue("conversation thread", mConversationThreads.size() > 0 ? string(mConversationThreads.size()) : String(), firstTime);
      }

      //-----------------------------------------------------------------------
      String Account::LocationSubscription::getPeerURI() const
      {
        static String empty;
        ContactSubscriptionPtr outer = mOuter.lock();
        if (outer) return outer->getContact()->forAccount().getPeerURI();
        return empty;
      }

      //-----------------------------------------------------------------------
      String Account::LocationSubscription::getLocationID() const
      {
        if (!mPeerLocation) return String();
        return mPeerLocation->getLocationID();
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::cancel()
      {
        if (isShutdown()) return;

        setState(LocationSubscriptionState_ShuttingDown);

        if (!mGracefulShutdownReference) mGracefulShutdownReference = mThisWeak.lock();

        // scope: notify all the conversation threads that the peer location is shutting down
        {
          for (ConversationThreadMap::iterator iter = mConversationThreads.begin(); iter != mConversationThreads.end(); ++iter)
          {
            ConversationThreadPtr &thread = (*iter).second;
            thread->forAccount().notifyPeerDisconnected(mPeerLocation);
          }
          mConversationThreads.clear();
        }

        if (mPublicationSubscription) {
          mPublicationSubscription->cancel();
        }

        if (mGracefulShutdownReference) {
          if (mPublicationSubscription) {
            if (stack::IPublicationSubscription::PublicationSubscriptionState_Shutdown != mPublicationSubscription->getState()) {
              ZS_LOG_DEBUG(log("waiting for publication subscription to shutdown"))
              return;
            }
          }
        }

        setState(LocationSubscriptionState_Shutdown);

        ContactSubscriptionPtr outer = mOuter.lock();
        if ((outer) &&
            (mPeerLocation)) {
          outer->notifyLocationShutdown(getLocationID());
        }

        mPublicationSubscription.reset();
        mPeerLocation.reset();
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::step()
      {
        if ((isShuttingDown()) ||
            (isShutdown())) {
          cancel();
          return;
        }

        if (!mPublicationSubscription) {
          ContactSubscriptionPtr outer = mOuter.lock();
          if (!outer) {
            ZS_LOG_WARNING(Detail, log("failed to obtain contact subscription"))
            return;
          }

          AccountPtr account = outer->getOuter();
          if (!account) {
            ZS_LOG_WARNING(Detail, log("failed to obtain account"))
            return;
          }

          stack::IAccountPtr stackAccount = account->getStackAccount();
          if (!stackAccount) {
            ZS_LOG_WARNING(Detail, log("failed to obtain stack account"))
            return;
          }

          IPublicationRepositoryPtr repository = account->getRepository();
          if (!repository) {
            ZS_LOG_WARNING(Detail, log("failed to obtain stack publication respository"))
            return;
          }

          stack::IPublicationMetaData::PeerURIList empty;
          stack::IPublicationRepository::SubscribeToRelationshipsMap relationships;
          relationships["/threads/1.0/subscribers/permissions"] = IPublicationMetaData::PermissionAndPeerURIListPair(stack::IPublicationMetaData::Permission_All, empty);

          ZS_LOG_DEBUG(log("subscribing to peer thread publications"))
          mPublicationSubscription = repository->subscribe(mThisWeak.lock(), mPeerLocation, "/threads/1.0/", relationships);
        }

        if (!mPublicationSubscription) {
          ZS_LOG_WARNING(Detail, log("failed to create publication subscription"))
          cancel();
          return;
        }

        if (IPublicationSubscription::PublicationSubscriptionState_Established != mPublicationSubscription->getState()) {
          ZS_LOG_DEBUG(log("waiting for publication subscription to establish"))
          return;
        }

        setState(LocationSubscriptionState_Ready);
      }

      //-----------------------------------------------------------------------
      void Account::LocationSubscription::setState(LocationSubscriptionStates state)
      {
        if (state == mCurrentState) return;

        ZS_LOG_BASIC(log("state changed") + ", old state=" + toString(mCurrentState) + ", new state=" + toString(state))

        mCurrentState = state;
      }
    }

    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    #pragma mark
    #pragma mark IAccount
    #pragma mark

    //-------------------------------------------------------------------------
    const char *IAccount::toString(AccountStates state)
    {
      switch (state) {
        case AccountState_Pending:                                return "Pending";
        case AccountState_PendingPeerFilesGeneration:             return "Pending Peer File Generation";
        case AccountState_WaitingForAssociationToIdentity:        return "Waiting for Association to Identity";
        case AccountState_WaitingForBrowserWindowToBeLoaded:      return "Waiting for Browser Window to be Loaded";
        case AccountState_WaitingForBrowserWindowToBeMadeVisible: return "Waiting for Browser Window to be made Visible";
        case AccountState_WaitingForBrowserWindowToClose:         return "Waiting for Browser Window to Close";
        case AccountState_Ready:                                  return "Ready";
        case AccountState_ShuttingDown:                           return "Shutting down";
        case AccountState_Shutdown:                               return "Shutdown";
      }
      return "UNDEFINED";
    }

    //-------------------------------------------------------------------------
    String IAccount::toDebugString(IAccountPtr account, bool includeCommaPrefix)
    {
      return internal::Account::toDebugString(account, includeCommaPrefix);
    }

    //-------------------------------------------------------------------------
    IAccountPtr IAccount::login(
                                IAccountDelegatePtr delegate,
                                IConversationThreadDelegatePtr conversationThreadDelegate,
                                ICallDelegatePtr callDelegate,
                                const char *namespaceGrantOuterFrameURLUponReload,
                                const char *grantID,
                                const char *lockboxServiceDomain,
                                bool forceCreateNewLockboxAccount
                                )
    {
      return internal::IAccountFactory::singleton().login(delegate, conversationThreadDelegate, callDelegate, namespaceGrantOuterFrameURLUponReload, grantID, lockboxServiceDomain, forceCreateNewLockboxAccount);
    }

    //-------------------------------------------------------------------------
    IAccountPtr IAccount::relogin(
                                  IAccountDelegatePtr delegate,
                                  IConversationThreadDelegatePtr conversationThreadDelegate,
                                  ICallDelegatePtr callDelegate,
                                  const char *namespaceGrantOuterFrameURLUponReload,
                                  ElementPtr reloginInformation
                                  )
    {
      return internal::IAccountFactory::singleton().relogin(delegate, conversationThreadDelegate, callDelegate, namespaceGrantOuterFrameURLUponReload, reloginInformation);
    }
  }
}
