/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_peer_values.h"

#include "lang/lang_keys.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_user.h"

namespace Data {
namespace {

constexpr auto kMinOnlineChangeTimeout = crl::time(1000);
constexpr auto kMaxOnlineChangeTimeout = 86400 * crl::time(1000);
constexpr auto kSecondsInDay = 86400;

int OnlinePhraseChangeInSeconds(TimeId online, TimeId now) {
	if (online <= 0) {
		if (-online > now) {
			return (-online - now);
		}
		return std::numeric_limits<TimeId>::max();
	}
	if (online > now) {
		return online - now;
	}
	const auto minutes = (now - online) / 60;
	if (minutes < 60) {
		return (minutes + 1) * 60 - (now - online);
	}
	const auto hours = (now - online) / 3600;
	if (hours < 12) {
		return (hours + 1) * 3600 - (now - online);
	}
	const auto nowFull = ParseDateTime(now);
	const auto tomorrow = QDateTime(nowFull.date().addDays(1));
	return std::max(static_cast<TimeId>(nowFull.secsTo(tomorrow)), 0);
}

std::optional<QString> OnlineTextSpecial(not_null<UserData*> user) {
	if (user->isNotificationsUser()) {
		return lang(lng_status_service_notifications);
	} else if (user->isSupport()) {
		return lang(lng_status_support);
	} else if (user->isBot()) {
		return lang(lng_status_bot);
	} else if (user->isServiceUser()) {
		return lang(lng_status_support);
	}
	return std::nullopt;
}

std::optional<QString> OnlineTextCommon(TimeId online, TimeId now) {
	if (online <= 0) {
		switch (online) {
		case 0:
		case -1: return lang(lng_status_offline);
		case -2: return lang(lng_status_recently);
		case -3: return lang(lng_status_last_week);
		case -4: return lang(lng_status_last_month);
		}
		return (-online > now)
			? lang(lng_status_online)
			: lang(lng_status_recently);
	} else if (online > now) {
		return lang(lng_status_online);
	}
	return std::nullopt;
}

} // namespace

inline auto AdminRightsValue(not_null<ChannelData*> channel) {
	return channel->adminRightsValue();
}

inline auto AdminRightsValue(
		not_null<ChannelData*> channel,
		MTPDchatAdminRights::Flags mask) {
	return FlagsValueWithMask(AdminRightsValue(channel), mask);
}

inline auto AdminRightValue(
		not_null<ChannelData*> channel,
		MTPDchatAdminRights::Flag flag) {
	return SingleFlagValue(AdminRightsValue(channel), flag);
}

inline auto AdminRightsValue(not_null<ChatData*> chat) {
	return chat->adminRightsValue();
}

inline auto AdminRightsValue(
		not_null<ChatData*> chat,
		MTPDchatAdminRights::Flags mask) {
	return FlagsValueWithMask(AdminRightsValue(chat), mask);
}

inline auto AdminRightValue(
		not_null<ChatData*> chat,
		MTPDchatAdminRights::Flag flag) {
	return SingleFlagValue(AdminRightsValue(chat), flag);
}

inline auto RestrictionsValue(not_null<ChannelData*> channel) {
	return channel->restrictionsValue();
}

inline auto RestrictionsValue(
		not_null<ChannelData*> channel,
		MTPDchatBannedRights::Flags mask) {
	return FlagsValueWithMask(RestrictionsValue(channel), mask);
}

inline auto RestrictionValue(
		not_null<ChannelData*> channel,
		MTPDchatBannedRights::Flag flag) {
	return SingleFlagValue(RestrictionsValue(channel), flag);
}

inline auto DefaultRestrictionsValue(not_null<ChannelData*> channel) {
	return channel->defaultRestrictionsValue();
}

inline auto DefaultRestrictionsValue(
		not_null<ChannelData*> channel,
		MTPDchatBannedRights::Flags mask) {
	return FlagsValueWithMask(DefaultRestrictionsValue(channel), mask);
}

inline auto DefaultRestrictionValue(
		not_null<ChannelData*> channel,
		MTPDchatBannedRights::Flag flag) {
	return SingleFlagValue(DefaultRestrictionsValue(channel), flag);
}

inline auto DefaultRestrictionsValue(not_null<ChatData*> chat) {
	return chat->defaultRestrictionsValue();
}

inline auto DefaultRestrictionsValue(
		not_null<ChatData*> chat,
		MTPDchatBannedRights::Flags mask) {
	return FlagsValueWithMask(DefaultRestrictionsValue(chat), mask);
}

inline auto DefaultRestrictionValue(
		not_null<ChatData*> chat,
		MTPDchatBannedRights::Flag flag) {
	return SingleFlagValue(DefaultRestrictionsValue(chat), flag);
}

rpl::producer<bool> PeerFlagValue(
		ChatData *chat,
		MTPDchat_ClientFlag flag) {
	return PeerFlagValue(chat, static_cast<MTPDchat::Flag>(flag));
}

rpl::producer<bool> PeerFlagValue(
		ChannelData *channel,
		MTPDchannel_ClientFlag flag) {
	return PeerFlagValue(channel, static_cast<MTPDchannel::Flag>(flag));
}

rpl::producer<bool> CanWriteValue(UserData *user) {
	using namespace rpl::mappers;
	return PeerFlagValue(user, MTPDuser::Flag::f_deleted)
		| rpl::map(!_1);
}

rpl::producer<bool> CanWriteValue(ChatData *chat) {
	using namespace rpl::mappers;
	const auto mask = 0
		| MTPDchat::Flag::f_deactivated
		| MTPDchat_ClientFlag::f_forbidden
		| MTPDchat::Flag::f_left
		| MTPDchat::Flag::f_creator
		| MTPDchat::Flag::f_kicked;
	return rpl::combine(
		PeerFlagsValue(chat, mask),
		AdminRightsValue(chat),
		DefaultRestrictionValue(
			chat,
			MTPDchatBannedRights::Flag::f_send_messages),
		[](
				MTPDchat::Flags flags,
				Data::Flags<ChatAdminRights>::Change adminRights,
				bool defaultSendMessagesRestriction) {
			const auto amOutFlags = 0
				| MTPDchat::Flag::f_deactivated
				| MTPDchat_ClientFlag::f_forbidden
				| MTPDchat::Flag::f_left
				| MTPDchat::Flag::f_kicked;
			return !(flags & amOutFlags)
				&& ((flags & MTPDchat::Flag::f_creator)
					|| (adminRights.value != MTPDchatAdminRights::Flags(0))
					|| !defaultSendMessagesRestriction);
		});
}

rpl::producer<bool> CanWriteValue(ChannelData *channel) {
	const auto mask = 0
		| MTPDchannel::Flag::f_left
		| MTPDchannel_ClientFlag::f_forbidden
		| MTPDchannel::Flag::f_creator
		| MTPDchannel::Flag::f_broadcast;
	return rpl::combine(
		PeerFlagsValue(channel, mask),
		AdminRightValue(
			channel,
			MTPDchatAdminRights::Flag::f_post_messages),
		RestrictionValue(
			channel,
			MTPDchatBannedRights::Flag::f_send_messages),
		DefaultRestrictionValue(
			channel,
			MTPDchatBannedRights::Flag::f_send_messages),
		[](
				MTPDchannel::Flags flags,
				bool postMessagesRight,
				bool sendMessagesRestriction,
				bool defaultSendMessagesRestriction) {
			const auto notAmInFlags = 0
				| MTPDchannel::Flag::f_left
				| MTPDchannel_ClientFlag::f_forbidden;
			return !(flags & notAmInFlags)
				&& (postMessagesRight
					|| (flags & MTPDchannel::Flag::f_creator)
					|| (!(flags & MTPDchannel::Flag::f_broadcast)
						&& !sendMessagesRestriction
						&& !defaultSendMessagesRestriction));
		});
}

rpl::producer<bool> CanWriteValue(not_null<PeerData*> peer) {
	if (auto user = peer->asUser()) {
		return CanWriteValue(user);
	} else if (auto chat = peer->asChat()) {
		return CanWriteValue(chat);
	} else if (auto channel = peer->asChannel()) {
		return CanWriteValue(channel);
	}
	Unexpected("Bad peer value in CanWriteValue()");
}

TimeId SortByOnlineValue(not_null<UserData*> user, TimeId now) {
	if (user->isServiceUser() || user->isBot()) {
		return -1;
	}
	const auto online = user->onlineTill;
	if (online <= 0) {
		switch (online) {
		case 0:
		case -1: return online;

		case -2: {
			return now - 3 * kSecondsInDay;
		} break;

		case -3: {
			return now - 7 * kSecondsInDay;
		} break;

		case -4: {
			return now - 30 * kSecondsInDay;
		} break;
		}
		return -online;
	}
	return online;
}

crl::time OnlineChangeTimeout(TimeId online, TimeId now) {
	const auto result = OnlinePhraseChangeInSeconds(online, now);
	Assert(result >= 0);
	return snap(
		result * crl::time(1000),
		kMinOnlineChangeTimeout,
		kMaxOnlineChangeTimeout);
}

crl::time OnlineChangeTimeout(not_null<UserData*> user, TimeId now) {
	if (user->isServiceUser() || user->botInfo) {
		return kMaxOnlineChangeTimeout;
	}
	return OnlineChangeTimeout(user->onlineTill, now);
}

QString OnlineText(TimeId online, TimeId now) {
	if (const auto common = OnlineTextCommon(online, now)) {
		return *common;
	}
	const auto minutes = (now - online) / 60;
	if (!minutes) {
		return lang(lng_status_lastseen_now);
	} else if (minutes < 60) {
		return lng_status_lastseen_minutes(lt_count, minutes);
	}
	const auto hours = (now - online) / 3600;
	if (hours < 12) {
		return lng_status_lastseen_hours(lt_count, hours);
	}
	const auto onlineFull = ParseDateTime(online);
	const auto nowFull = ParseDateTime(now);
	if (onlineFull.date() == nowFull.date()) {
		const auto onlineTime = onlineFull.time().toString(cTimeFormat());
		return lng_status_lastseen_today(lt_time, onlineTime);
	} else if (onlineFull.date().addDays(1) == nowFull.date()) {
		const auto onlineTime = onlineFull.time().toString(cTimeFormat());
		return lng_status_lastseen_yesterday(lt_time, onlineTime);
	}
	const auto date = onlineFull.date().toString(qsl("dd.MM.yy"));
	return lng_status_lastseen_date(lt_date, date);
}

QString OnlineText(not_null<UserData*> user, TimeId now) {
	if (const auto special = OnlineTextSpecial(user)) {
		return *special;
	}
	return OnlineText(user->onlineTill, now);
}

QString OnlineTextFull(not_null<UserData*> user, TimeId now) {
	if (const auto special = OnlineTextSpecial(user)) {
		return *special;
	} else if (const auto common = OnlineTextCommon(user->onlineTill, now)) {
		return *common;
	}
	const auto onlineFull = ParseDateTime(user->onlineTill);
	const auto nowFull = ParseDateTime(now);
	if (onlineFull.date() == nowFull.date()) {
		const auto onlineTime = onlineFull.time().toString(cTimeFormat());
		return lng_status_lastseen_today(lt_time, onlineTime);
	} else if (onlineFull.date().addDays(1) == nowFull.date()) {
		const auto onlineTime = onlineFull.time().toString(cTimeFormat());
		return lng_status_lastseen_yesterday(lt_time, onlineTime);
	}
	const auto date = onlineFull.date().toString(qsl("dd.MM.yy"));
	const auto time = onlineFull.time().toString(cTimeFormat());
	return lng_status_lastseen_date_time(lt_date, date, lt_time, time);
}

bool OnlineTextActive(TimeId online, TimeId now) {
	if (online <= 0) {
		switch (online) {
		case 0:
		case -1:
		case -2:
		case -3:
		case -4: return false;
		}
		return (-online > now);
	}
	return (online > now);
}

bool OnlineTextActive(not_null<UserData*> user, TimeId now) {
	if (user->isServiceUser() || user->botInfo) {
		return false;
	}
	return OnlineTextActive(user->onlineTill, now);
}

bool IsPeerAnOnlineUser(not_null<PeerData*> peer) {
	if (const auto user = peer->asUser()) {
		return OnlineTextActive(user, unixtime());
	}
	return false;
}

} // namespace Data
