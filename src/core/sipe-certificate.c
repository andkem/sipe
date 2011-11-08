/**
 * @file sipe-certificate.c
 *
 * pidgin-sipe
 *
 * Copyright (C) 2011 SIPE Project <http://sipe.sourceforge.net/>
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
 *
 *
 * Specification references:
 *
 *   - [MS-SIPAE]:    http://msdn.microsoft.com/en-us/library/cc431510.aspx
 *   - [MS-OCAUTHWS]: http://msdn.microsoft.com/en-us/library/ff595592.aspx
 *   - MS Tech-Ed Europe 2010 "UNC310: Microsoft Lync 2010 Technology Explained"
 *     http://ecn.channel9.msdn.com/o9/te/Europe/2010/pptx/unc310.pptx
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <glib.h>

#include "sipe-common.h"
#include "sipe-backend.h"
#include "sipe-core.h"
#include "sipe-core-private.h"
#include "sipe-certificate.h"
#include "sipe-nls.h"
#include "sipe-svc.h"
#include "sipe-utils.h"
#include "sipe-xml.h"

struct certificate_callback_data {
	gchar *target;
	gchar *authuser;
	gchar *webticket_anon_uri;
	gchar *webticket_fedbearer_uri;
	gchar *certprov_uri;

	gboolean tried_fedbearer;
	gboolean webticket_for_certprov;

	struct sipe_svc_random entropy;
};

static void callback_data_free(struct certificate_callback_data *ccd)
{
	if (ccd) {
		g_free(ccd->target);
		g_free(ccd->authuser);
		g_free(ccd->webticket_anon_uri);
		g_free(ccd->webticket_fedbearer_uri);
		g_free(ccd->certprov_uri);
		sipe_svc_free_random(&ccd->entropy);
		g_free(ccd);
	}
}

gpointer sipe_certificate_tls_dsk_find(struct sipe_core_private *sipe_private,
				       const gchar *target)
{
	if (!target)
		return(NULL);

	/* temporary */
	(void)sipe_private;

	return(NULL);
}

static void certificate_failure(struct sipe_core_private *sipe_private,
				const gchar *format,
				const gchar *parameter)
{
	gchar *tmp = g_strdup_printf(format, parameter);
	sipe_backend_connection_error(SIPE_CORE_PUBLIC,
				      SIPE_CONNECTION_ERROR_AUTHENTICATION_FAILED,
				      tmp);
	g_free(tmp);
}

static void get_and_publish_cert(struct sipe_core_private *sipe_private,
				 const gchar *uri,
				 const gchar *raw,
				 sipe_xml *soap_body,
				 gpointer callback_data)
{
	struct certificate_callback_data *ccd = callback_data;

	if (soap_body) {

		/* TBD.... */
		(void)raw;

	} else if (uri) {
		certificate_failure(sipe_private,
				    _("Certitifcate request to %s failed"),
				    uri);
	}

	callback_data_free(ccd);
}

static gchar *extract_raw_xml(const gchar *xml,
			      const gchar *tag,
			      gboolean include_tag)
{
	gchar *tag_start = g_strdup_printf("<%s", tag);
	gchar *tag_end   = g_strdup_printf("</%s>", tag);
	gchar *data      = NULL;
	const gchar *start = strstr(xml, tag_start);

	if (start) {
		const gchar *end = strstr(start + strlen(tag_start), tag_end);
		if (end) {
			if (include_tag) {
				data = g_strndup(start, end + strlen(tag_end) - start);
			} else {
				const gchar *tmp = strchr(start + strlen(tag_start), '>') + 1;
				data = g_strndup(tmp, end - tmp);
			}
		}
	}

	g_free(tag_end);
	g_free(tag_start);
	return(data);
}

static gchar *generate_wsse_security(const gchar *raw,
				     const gchar *lifetime_tag,
				     const gchar *keydata_tag)
{
	gchar *lifetime = extract_raw_xml(raw, lifetime_tag, FALSE);
	gchar *keydata  = extract_raw_xml(raw, keydata_tag, TRUE);
	gchar *wsse_security = NULL;

	if (lifetime && keydata) {
		wsse_security = g_strdup_printf("<wsu:Timestamp>%s</wsu:Timestamp>%s",
						lifetime, keydata);
	}

	g_free(keydata);
	g_free(lifetime);

	return(wsse_security);
}

static void webticket_token(struct sipe_core_private *sipe_private,
			    const gchar *uri,
			    const gchar *raw,
			    sipe_xml *soap_body,
			    gpointer callback_data)
{
	struct certificate_callback_data *ccd = callback_data;
	gboolean success = (uri == NULL); /* abort case */

	if (soap_body) {
		/* WebTicket for Certificate Provisioning Service */
		if (ccd->webticket_for_certprov) {
			gchar *wsse_security = generate_wsse_security(raw,
								      "Lifetime",
								      "saml:Assertion");
			if (wsse_security) {

				SIPE_DEBUG_INFO("webticket_token: received valid SOAP message from service %s",
						uri);

				success = sipe_svc_get_and_publish_cert(sipe_private,
									ccd->certprov_uri,
									ccd->authuser,
									wsse_security,
									"", /* TBD.... */
									get_and_publish_cert,
									ccd);
				if (success) {
					/* callback data passed down the line */
					ccd = NULL;
				}
				g_free(wsse_security);
			}

		/* WebTicket for federated authentication */
		} else {
			gchar *wsse_security = generate_wsse_security(raw,
								      "wst:Lifetime",
								      "EncryptedData");
			if (wsse_security) {

				SIPE_DEBUG_INFO("webticket_token: received valid SOAP message from service %s",
						uri);

				success = sipe_svc_webticket(sipe_private,
							     ccd->webticket_fedbearer_uri,
							     ccd->authuser,
							     wsse_security,
							     ccd->certprov_uri,
							     &ccd->entropy,
							     webticket_token,
							     ccd);
				ccd->webticket_for_certprov = TRUE;

				if (success) {
					/* callback data passed down the line */
					ccd = NULL;
				}
				g_free(wsse_security);
			}
		}

	} else if (uri) {
		/* Retry with federated authentication? */
		success = ccd->webticket_fedbearer_uri && !ccd->tried_fedbearer;
		if (success) {
			SIPE_DEBUG_INFO("webticket_token: anonymous authentication to service %s failed, retrying with federated authentication",
					uri);

			ccd->tried_fedbearer = TRUE;
			success = sipe_svc_webticket_lmc(sipe_private,
							 ccd->authuser,
							 ccd->webticket_fedbearer_uri,
							 webticket_token,
							 ccd);
			ccd->webticket_for_certprov = FALSE;

			if (success) {
				/* callback data passed down the line */
				ccd = NULL;
			}
		}
	}

	if (!success) {
		certificate_failure(sipe_private,
				    _("Web ticket request to %s failed"),
				    uri);
	}

	callback_data_free(ccd);
}

static void webticket_metadata(struct sipe_core_private *sipe_private,
			       const gchar *uri,
			       SIPE_UNUSED_PARAMETER const gchar *raw,
			       sipe_xml *metadata,
			       gpointer callback_data)
{
	struct certificate_callback_data *ccd = callback_data;

	if (metadata) {
		const sipe_xml *node;

		SIPE_DEBUG_INFO("webticket_metadata: metadata for service %s retrieved successfully",
				uri);

		/* Authentication ports accepted by WebTicket Service */
		for (node = sipe_xml_child(metadata, "service/port");
		     node;
		     node = sipe_xml_twin(node)) {
			const gchar *auth_uri = sipe_xml_attribute(sipe_xml_child(node,
										  "address"),
								   "location");

			if (auth_uri) {
				if (sipe_strcase_equal(sipe_xml_attribute(node, "name"),
						       "WebTicketServiceAnon")) {
					SIPE_DEBUG_INFO("webticket_metadata: WebTicket Anon Auth URI %s", auth_uri);
					g_free(ccd->webticket_anon_uri);
					ccd->webticket_anon_uri = g_strdup(auth_uri);
				} else if (sipe_strcase_equal(sipe_xml_attribute(node, "name"),
							      "WsFedBearer")) {
					SIPE_DEBUG_INFO("webticket_metadata: WebTicket FedBearer Auth URI %s", auth_uri);
					g_free(ccd->webticket_fedbearer_uri);
					ccd->webticket_fedbearer_uri = g_strdup(auth_uri);
				}
			}
		}

		if (ccd->webticket_anon_uri || ccd->webticket_fedbearer_uri) {
			gboolean success;

			if (ccd->webticket_anon_uri) {
				/* Try anonymous authentication first */
				/* Entropy: 256 random bits */
				sipe_svc_fill_random(&ccd->entropy, 256);

				success = sipe_svc_webticket(sipe_private,
							     ccd->webticket_anon_uri,
							     ccd->authuser,
							     NULL,
							     ccd->certprov_uri,
							     &ccd->entropy,
							     webticket_token,
							     ccd);
				ccd->webticket_for_certprov = TRUE;
			} else {
				ccd->tried_fedbearer = TRUE;
				success = sipe_svc_webticket_lmc(sipe_private,
								 ccd->authuser,
								 ccd->webticket_fedbearer_uri,
								 webticket_token,
								 ccd);
				ccd->webticket_for_certprov = FALSE;
			}

			if (success) {
				/* callback data passed down the line */
				ccd = NULL;
			} else {
				certificate_failure(sipe_private,
						    _("Can't request security token from %s"),
						    ccd->webticket_anon_uri ? ccd->webticket_anon_uri : ccd->webticket_fedbearer_uri);
			}

		} else {
			certificate_failure(sipe_private,
					    _("Can't find the authentication port for TLS-DSK web ticket URI %s"),
					    uri);
		}

	} else if (uri) {
		certificate_failure(sipe_private,
				    _("Can't retrieve metadata for TLS-DSK web ticket URI %s"),
				    uri);
	}

	callback_data_free(ccd);
}

static void certprov_metadata(struct sipe_core_private *sipe_private,
			      const gchar *uri,
			      SIPE_UNUSED_PARAMETER const gchar *raw,
			      sipe_xml *metadata,
			      gpointer callback_data)
{
	struct certificate_callback_data *ccd = callback_data;

	if (metadata) {
		const sipe_xml *node;
		gchar *ticket_uri = NULL;

		SIPE_DEBUG_INFO("certprov_metadata: metadata for service %s retrieved successfully",
				uri);

		/* WebTicket policies accepted by Certificate Provisioning Service */
		for (node = sipe_xml_child(metadata, "Policy");
		     node;
		     node = sipe_xml_twin(node)) {
			if (sipe_strcase_equal(sipe_xml_attribute(node, "Id"),
					       "CertProvisioningServiceWebTicketProof_SHA1_policy")) {

				SIPE_DEBUG_INFO_NOFORMAT("certprov_metadata: WebTicket policy found");

				ticket_uri = sipe_xml_data(sipe_xml_child(node,
									  "ExactlyOne/All/EndorsingSupportingTokens/Policy/IssuedToken/Issuer/Address"));
				if (ticket_uri) {
					SIPE_DEBUG_INFO("certprov_metadata: WebTicket URI %s", ticket_uri);
				} else {
					certificate_failure(sipe_private,
							    _("Can't find the WebTicket URI for TLS-DSK certificate provisioning URI %s"),
							    uri);
				}
				break;
			}
		}

		if (ticket_uri) {

			/* Authentication ports accepted by Certificate Provisioning Service */
			for (node = sipe_xml_child(metadata, "service/port");
			     node;
			     node = sipe_xml_twin(node)) {
				if (sipe_strcase_equal(sipe_xml_attribute(node, "name"),
						       "CertProvisioningServiceWebTicketProof_SHA1")) {
					const gchar *auth_uri;

					SIPE_DEBUG_INFO_NOFORMAT("certprov_metadata: authentication port found");

					auth_uri = sipe_xml_attribute(sipe_xml_child(node,
										     "address"),
								      "location");
					if (auth_uri) {
						SIPE_DEBUG_INFO("certprov_metadata: CertProv Auth URI %s", auth_uri);

						if (sipe_svc_metadata(sipe_private,
								      ticket_uri,
								      webticket_metadata,
								      ccd)) {
							/* Remember for later */
							ccd->certprov_uri = g_strdup(auth_uri);

							/* callback data passed down the line */
							ccd = NULL;
						} else {
							certificate_failure(sipe_private,
									    _("Can't request metadata from %s"),
									    ticket_uri);
						}
					}
					break;
				}
			}

			g_free(ticket_uri);

			if (!node) {
				certificate_failure(sipe_private,
						    _("Can't find the authentication port for TLS-DSK certificate provisioning URI %s"),
						    uri);
			}

		} else {
			certificate_failure(sipe_private,
					    _("Can't find the WebTicket Policy for TLS-DSK certificate provisioning URI %s"),
					    uri);
		}

	} else if (uri) {
		certificate_failure(sipe_private,
				    _("Can't retrieve metadata for TLS-DSK certificate provisioning URI %s"),
				    uri);
	}

	callback_data_free(ccd);
}

gboolean sipe_certificate_tls_dsk_generate(struct sipe_core_private *sipe_private,
					   const gchar *target,
					   const gchar *authuser,
					   const gchar *uri)
{
	struct certificate_callback_data *ccd = g_new0(struct certificate_callback_data, 1);
	gboolean ret;

	ccd->target   = g_strdup(target);
	ccd->authuser = g_strdup(authuser);

	ret = sipe_svc_metadata(sipe_private, uri, certprov_metadata, ccd);
	if (!ret)
		callback_data_free(ccd);

	return(ret);
}

/*
  Local Variables:
  mode: c
  c-file-style: "bsd"
  indent-tabs-mode: t
  tab-width: 8
  End:
*/