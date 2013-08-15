/**
 * @file sipe-ucs.h
 *
 * pidgin-sipe
 *
 * Copyright (C) 2013 SIPE Project <http://sipe.sourceforge.net/>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Forward declarations */
struct sipe_core_private;
struct sipe_group;

/**
 * Get buddy photo
 *
 * This is not directly related to UCS, but we can reuse the code.
 *
 * @param sipe_private SIPE core private data
 * @param uri          SIP URI of the user
 */
void sipe_ucs_get_photo(struct sipe_core_private *sipe_private,
			const gchar *uri);

/**
 * Has contact list been migrated to UCS?
 *
 * @param sipe_private SIPE core private data
 *
 * @return @c TRUE if contact list has been migrated
 */
gboolean sipe_ucs_is_migrated(struct sipe_core_private *sipe_private);

/**
 * Rename group
 *
 * @param sipe_private SIPE core private data
 * @param group        sipe_group structure
 * @param new_name     new name for group
 */
void sipe_ucs_group_rename(struct sipe_core_private *sipe_private,
			   struct sipe_group *group,
			   const gchar *new_name);

/**
 * Remove group
 *
 * @param sipe_private SIPE core private data
 * @param group        sipe_group structure
 */
void sipe_ucs_group_remove(struct sipe_core_private *sipe_private,
			   struct sipe_group *group);

/**
 * Initialize UCS
 *
 * @param sipe_private SIPE core private data
 * @param migrated     @c TRUE if contact list has been migrated
 */
void sipe_ucs_init(struct sipe_core_private *sipe_private,
		   gboolean migrated);

/**
 * Free UCS data
 *
 * @param sipe_private SIPE core private data
 */
void sipe_ucs_free(struct sipe_core_private *sipe_private);
