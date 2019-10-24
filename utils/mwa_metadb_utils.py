#!/usr/bin/env python3

import logging
import argparse

logger = logging.getLogger(__name__)


def get_obs_array_phase(obsid):
    """
    For the input obsid will work out the observations array phase in the form
    of P1 for phase 1, P2C for phase 2 compact or P2E for phase to extended array
    and OTH for other.
    """
    phase_info = getmeta(service='con', params={'obs_id':obsid, 'summary':''})

    if phase_info[0] == "PHASE1":
        return "P1"
    elif phase_info[0] == "COMPACT":
        return "P2C"
    elif phase_info[0] == "LB":
        return "P2E"
    elif phase_info[0] == "OTHER":
        return "OTH"
    else:
        logger.error("Unknown phase: {0}. Exiting".format(phase_info[0]))
        exit()


def mwa_alt_az_za(obsid, ra=None, dec=None, degrees=False):
    """
    Calculate the altitude, azumith and zenith for an obsid

    Args:
        obsid  : The MWA observation id (GPS time)
        ra     : The right acension in HH:MM:SS
        dec    : The declintation in HH:MM:SS
        degrees: If true the ra and dec is given in degrees (Default:False)
    """
    from astropy.time import Time
    from astropy.coordinates import SkyCoord, AltAz, EarthLocation
    from astropy import units as u

    obstime = Time(float(obsid),format='gps')

    if ra is None or dec is None:
        #if no ra and dec given use obsid ra and dec
        ra, dec = get_common_obs_metadata(obsid)[1:3]

    if degrees:
        sky_posn = SkyCoord(ra, dec, unit=(u.deg,u.deg))
    else:
        sky_posn = SkyCoord(ra, dec, unit=(u.hourangle,u.deg))
    #earth_location = EarthLocation.of_site('Murchison Widefield Array')
    earth_location = EarthLocation.from_geodetic(lon="116:40:14.93", lat="-26:42:11.95", height=377.8)
    altaz = sky_posn.transform_to(AltAz(obstime=obstime, location=earth_location))
    Alt = altaz.alt.deg
    Az  = altaz.az.deg
    Za  = 90. - Alt
    return Alt, Az, Za


def get_common_obs_metadata(obs, return_all = False):
    """
    Gets needed comon meta data from http://ws.mwatelescope.org/metadata/
    """
    logger.info("Obtaining metadata from http://ws.mwatelescope.org/metadata/ for OBS ID: " + str(obs))
    #for line in txtfile:
    beam_meta_data = getmeta(service='obs', params={'obs_id':obs})
    #obn = beam_meta_data[u'obsname']
    ra = beam_meta_data[u'metadata'][u'ra_pointing'] #in sexidecimal
    dec = beam_meta_data[u'metadata'][u'dec_pointing']
    dura = beam_meta_data[u'stoptime'] - beam_meta_data[u'starttime'] #gps time
    xdelays = beam_meta_data[u'rfstreams'][u"0"][u'xdelays']
    ydelays = beam_meta_data[u'rfstreams'][u"0"][u'ydelays']
    minfreq = float(min(beam_meta_data[u'rfstreams'][u"0"][u'frequencies']))
    maxfreq = float(max(beam_meta_data[u'rfstreams'][u"0"][u'frequencies']))
    channels = beam_meta_data[u'rfstreams'][u"0"][u'frequencies']
    centrefreq = 1.28 * (minfreq + (maxfreq-minfreq)/2)

    if return_all:
        return [obs, ra, dec, dura, [xdelays, ydelays], centrefreq, channels], beam_meta_data
    else:
        return [obs, ra, dec, dura, [xdelays, ydelays], centrefreq, channels]


def getmeta(service='obs', params=None):
    """
    Function to call a JSON web service and return a dictionary:
    Given a JSON web service ('obs', find, or 'con') and a set of parameters as
    a Python dictionary, return a Python dictionary xcontaining the result.
    Taken verbatim from http://mwa-lfd.haystack.mit.edu/twiki/bin/view/Main/MetaDataWeb
    """
    import urllib.request
    import json

    # Append the service name to this base URL, eg 'con', 'obs', etc.
    BASEURL = 'http://ws.mwatelescope.org/metadata/'


    if params:
        # Turn the dictionary into a string with encoded 'name=value' pairs
        data = urllib.parse.urlencode(params)
    else:
        data = ''

    if service.strip().lower() in ['obs', 'find', 'con']:
        service = service.strip().lower()
    else:
        logger.error("invalid service name: %s" % service)
        return

    try:
        result = json.load(urllib.request.urlopen(BASEURL + service + '?' + data))
    except urllib.error.HTTPError as err:
        logger.error("HTTP error from server: code=%d, response:\n %s" % (err.code, err.read()))
        return
    except urllib.error.URLError as err:
        logger.error("URL or network error: %s" % err.reason)
        return

    return result


def get_channels(obsid, channels=None):
    """
    Gets the channels ids from the observation's metadata. If channels is not None assumes the
    channels have already been aquired so it doesn't do an unnecessary database call.
    """
    if channels is None:
        print("Obtaining frequency channel data from http://mwa-metadata01.pawsey.org.au/metadata/"
              "for OBS ID: {}".format(obsid))
        beam_meta_data = getmeta(service='obs', params={'obs_id':obsid})
        channels = beam_meta_data[u'rfstreams'][u"0"][u'frequencies']
    return channels


def is_number(s):
    try:
        int(s)
        return True
    except ValueError:
        return False


def obs_max_min(obs_id):
    """
    Small function to query the database and return the times of the first and last file
    """
    obsinfo = getmeta(service='obs', params={'obs_id':obs_id})

    # Make a list of gps times excluding non-numbers from list
    times = [f[11:21] for f in obsinfo['files'].keys() if is_number(f[11:21])]
    obs_start = int(min(times))
    obs_end = int(max(times))
    return obs_start, obs_end

def write_obs_info(obs_id):
    """
    Writes obs info to a file in the current direcory
    """
    data_dict = getmeta(service='obs', params={'obs_id':str(obs_id)})
    filename = str(obs_id)+"_info.txt"
    logger.info("Writing to file: {0}".format(filename))
    data_dict = getmeta(params={"obsid":args.obsid})

    f = open(filename, "w+")
    f.write("-------------------------    Obs Info    --------------------------\n")
    f.write("Obs Name:        {}\n".format(data_dict["obsname"]))
    f.write("Creator:         {}\n".format(data_dict["rfstreams"]["0"]["creator"]))
    f.write("Start time:      {}\n".format(data_dict["starttime"]))
    f.write("Stop time:       {}\n".format(data_dict["stoptime"]))
    f.write("Duration:        {}\n".format(data_dict["stoptime"]-data_dict["starttime"]))
    f.write("RA Pointing:     {}\n".format(data_dict["metadata"]["ra_pointing"]))
    f.write("DEC Pointing:    {}\n".format(data_dict["metadata"]["dec_pointing"]))
    f.write("Data Ready?:     {}\n".format(data_dict["dataready"]))
    f.write("Channels:        {}\n".format(data_dict["rfstreams"]["0"]["frequencies"]))
    f.close()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="""Returns information on an input Obs ID""")
    parser.add_argument("obsid", type=int, help="Input Observation ID")
    parser.add_argument("-w", "--write", action="store_true", help="OPTIONAL - Use to write results to file.")
    args = parser.parse_args()

    if args.write == False:
        data_dict = getmeta(params={"obsid":args.obsid})
        print("-------------------------    Obs Info    --------------------------")
        print("Obs Name:        {}".format(data_dict["obsname"]))
        print("Creator:         {}".format(data_dict["rfstreams"]["0"]["creator"]))
        print("Start time:      {}".format(data_dict["starttime"]))
        print("Stop time:       {}".format(data_dict["stoptime"]))
        print("Duration:        {}".format(data_dict["stoptime"]-data_dict["starttime"]))
        print("RA Pointing:     {}".format(data_dict["metadata"]["ra_pointing"]))
        print("DEC Pointing:    {}".format(data_dict["metadata"]["dec_pointing"]))
        print("Data Ready?:     {}".format(data_dict["dataready"]))
        print("Channels:        {}".format(data_dict["rfstreams"]["0"]["frequencies"]))
    else:
        write_obs_info(args.obsid)
