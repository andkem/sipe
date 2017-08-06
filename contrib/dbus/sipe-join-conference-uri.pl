#!/usr/bin/perl -w
#
# @file sipe-join-conference-uri.pl
#
# pidgin-sipe
#
# Copyright (C) 2017 SIPE Project <http://sipe.sourceforge.net/>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#
# Test code for D-Bus interface "SipeJoinConferenceUri"
#
use 5.024;
use strict;
use warnings;

use Net::DBus;

# Check command line parameters
die "usage: $0 <conference URI> [<conference URI> ...]\n"
    unless @ARGV;

# Connect to libpurple over the session bus
my $bus     = Net::DBus->session;
my $service = $bus->get_service('im.pidgin.purple.PurpleService');
my $purple  = $service->get_object('/im/pidgin/purple/PurpleObject',
				   'im.pidgin.purple.PurpleInterface');

# Get list of enabled accounts
my $accounts = $purple->PurpleAccountsGetAllActive();
for my $accountId (@{ $accounts }) {
    my $username     = $purple->PurpleAccountGetUsername($accountId);
    my $protocolId   = $purple->PurpleAccountGetProtocolId($accountId);
    my $protocolName = $purple->PurpleAccountGetProtocolName($accountId);
    my $connectionId = $purple->PurpleAccountGetConnection($accountId);
    print "found account ${accountId}: ${username} (${protocolId}/${protocolName}, ${connectionId})\n";

    # Filter out SIPE accounts that are online
    if (($protocolId eq 'prpl-sipe') && ($connectionId != 0)) {

	# Filter out SIPE accounts that are really connected
	if ($purple->PurpleConnectionIsConnected($connectionId)) {

	    # Call interface for each parameter on the command line
	    for my $uri (@ARGV) {
		print "Trying to join conference '${uri}' on SIPE account '${username}'...\n";
		$purple->SipeJoinConferenceUri($accountId, $uri);
	    }
	}
    }
}

# That's all folks...
exit 0;